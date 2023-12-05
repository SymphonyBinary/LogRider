// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import { TextDecoder } from 'util';
import * as vscode from 'vscode';
import { LogRiderStateProvider } from './logRiderStateView';
import { WorldState, StackNode, LoggedObject } from './worldState';

// this method is called when your extension is activated
// your extension is activated the very first time the command is executed
export function activate(context: vscode.ExtensionContext) {

	const provider = new LogRiderProvider();
	context.subscriptions.push(vscode.workspace.registerTextDocumentContentProvider(provider.scheme, provider));

	let disposable = vscode.commands.registerCommand('logrider.openCLog', async () => {
		const clogURI = await vscode.window.showOpenDialog({
			openLabel: "Select clog file",
			canSelectMany: false,
			filters: { "captainsLog": ['clog']}
		});

		if(clogURI && clogURI[0]) {
			const virtualDocURI = vscode.Uri.parse('logRider:' + clogURI[0]);
			provider.refresh(virtualDocURI);
			const doc = await vscode.workspace.openTextDocument(virtualDocURI); // calls back into the provider
			await vscode.window.showTextDocument(doc, { preview: false });
			provider.startUpdateLoop();
		}
	});

	context.subscriptions.push(disposable);
	
	vscode.window.registerTreeDataProvider('logRiderState', provider.treeViewProvider);
}

class LogRiderProvider implements vscode.TextDocumentContentProvider {
	// emitter and its event
	scheme = "logRider";
	onDidChangeEmitter = new vscode.EventEmitter<vscode.Uri>();
	onDidChange = this.onDidChangeEmitter.event;

	intervalHandle : NodeJS.Timeout | undefined;

	worldState = new WorldState();

	lastStackNode: StackNode | undefined;

	treeViewProvider = new LogRiderStateProvider();

	//HACK: should watch for the window closing to clear data and refresh.
	loaded = false;

	refresh(uri: vscode.Uri) {
		if(this.loaded) { //HACK to not do anything the first time this is called before the window is opened.
			this.worldState = new WorldState();
			this.lastStackNode = undefined;
			this.treeViewProvider.onStackNodeChanged(null);
			this.treeViewProvider.refresh();
			this.onDidChangeEmitter.fire(uri);
		}
	}

	async provideTextDocumentContent(uri: vscode.Uri): Promise<string> {
		//TODO: this is tricky if we have more than one document open.  We need to keep a map
		// of all the logs open.


		//this gets us the entire uri, with the scheme too
		//return uri.toString();
		const filePath = vscode.Uri.parse(uri.fsPath);
		const encodedDoc = await vscode.workspace.fs.readFile(filePath);

		let dec = new TextDecoder("utf-8");
		let doc = dec.decode(encodedDoc);

		// construct the stack nodes
		var lines = doc.split('\n');
		let newDoc = "";
		let logLineNumber = 0;
		let currentProcessToStackNodeMap = new Map<string, Map<string, StackNode>>();
		let uniqueProcessIDCounter = 0;
		let uniqueProcessIDMap = new Map<string, number>();
		for(let i = 0; i < lines.length; i++) {
			const clogMatch = lines[i].match(/.*CAP_LOG : P=(.+?) T=(.*?) (.+?) (.*?) (\[.*?\])(.*)/);
			if(clogMatch !== null) {
				const processId = clogMatch[1];
				const threadId = clogMatch[2];
				let indentation = clogMatch[3];
				const depth = indentation.length;
				const functionId = clogMatch[4];
				const lineNumber = clogMatch[5];
				let infoString = clogMatch[6];
				
				indentation = indentation.replace(/:/g, "║");
				indentation = indentation.replace("F", "╔");
				indentation = indentation.replace("L", "╚");
				indentation = indentation.replace("-", "╠");
				indentation = indentation.replace(">", "╾");

				let uniqueProcessId = uniqueProcessIDMap.get(processId);
				if(uniqueProcessId === undefined) {
					uniqueProcessId = uniqueProcessIDCounter;
					uniqueProcessIDMap.set(processId, uniqueProcessId);
					++uniqueProcessIDCounter;
				}

				let processMap = currentProcessToStackNodeMap.get(processId);
				if(processMap === undefined) {
					processMap = new Map<string, StackNode>();
					currentProcessToStackNodeMap.set(processId, processMap);
				}

				//determine who is the "caller" (ie. the previous logged line with less depth than this one)
				let caller = processMap.get(threadId);
				if(caller !== undefined) {
					if(Math.abs(depth - caller.depth) > 1) {
						console.error("unexpected jump in depth on line " + (i+1));
						vscode.window.showErrorMessage("unexpected jump in depth on line " + (i+1));
					}

					for(; caller && (depth - 1) < caller.depth; caller = caller.caller) {}
				}

				const infoStringBlockMatch = infoString.match(/::\[(.*)\]::\[(.*)\] ([0-9a-z]+)/);
				if(infoStringBlockMatch !== null) {
					const filename = infoStringBlockMatch[1];
					let functionName = infoStringBlockMatch[2];
					let thisAddress = infoStringBlockMatch[3];

					functionName = functionName.replace(/.*[ :]+([a-zA-Z0-9_]*::[a-zA-Z0-9_]*)\(.*/g, "$1()");
					if(thisAddress === "0") {
						thisAddress = "";
					}

					let loggedObject = this.worldState.getOrCreateLoggedObject(thisAddress);
					let stackNode = new StackNode(logLineNumber, depth, filename, functionName, caller, loggedObject);

					this.worldState.pushStackNode(stackNode);
					processMap.set(threadId, stackNode);
					newDoc += uniqueProcessId + " " + threadId + " " + indentation + " " + functionId + " " + lineNumber + "::[" + filename + "]::["
						+ functionName + "] " + thisAddress + "\n";
					++logLineNumber;
					continue;
				} 

				const infoStringCommandMatch = infoString.match(/ (.*?): (.*)/);
				if(infoStringCommandMatch !== null) {
					if(caller === undefined) {
						throw new Error("push found without caller");
					}

					const command = infoStringCommandMatch[1];
					const payload = infoStringCommandMatch[2];
					
					if(command === "SET") {
						const setCommandMatch = payload.match( /(.*) = (.*)/);
						if(setCommandMatch) {
							const varName = setCommandMatch[1];
							const varValue = setCommandMatch[2]; 
							let maybeCurrent = caller.loggedObject?.pushedVariables.get(varName);
							if(maybeCurrent) {
								maybeCurrent.set(logLineNumber, varValue);
							} else {
								caller.loggedObject?.pushedVariables.set(varName, new Map([[logLineNumber, varValue]]));
							}
						} else {
							console.error("unrecognized SET command");
						}
					}
					
					let stackNode = new StackNode(logLineNumber, caller.depth, caller.fileName, caller.functionName, caller.caller, caller.loggedObject);
					this.worldState.pushStackNode(stackNode);
					processMap.set(threadId, stackNode);
					newDoc += uniqueProcessId + " " + threadId + " " + indentation + " " + functionId + " " + lineNumber + " " + command + ": " + payload + "\n";
					++logLineNumber;
					continue;
				}
			}
		}

		this.loaded = true;
		return newDoc;
	}

	startUpdateLoop() {
		this.intervalHandle = setInterval(() => {
			this.updateStateView();
		}, 500);
	}

	updateStateView() {
		if(this.intervalHandle === undefined) {
			throw new Error("undefined interval handler, while inside updater").stack;
		}

		const editor = vscode.window.activeTextEditor;

		if (!editor) {
			clearInterval(this.intervalHandle);
			return; // no editor
		}

		const document = editor.document;
		if (document.uri.scheme !== this.scheme) {
			// Not using this because I want to be able to switch
			// however, we don't want the ticks to do anything right now.
			//clearInterval(this.intervalHandle);
			return; // not my scheme
		}

		const position = editor.selection.active;
		
		let stackNode = this.worldState.getStackNodeAt(position.line);

		if(stackNode && this.lastStackNode !== stackNode) {
			//console.log("changed node:" + stackNode.line + " depth:" + stackNode.depth + " thisAddress:" + stackNode.loggedObject?.thisAddress);
			this.treeViewProvider.onStackNodeChanged(stackNode);
			this.lastStackNode = stackNode;
		}
	}
}

// this method is called when your extension is deactivated
export function deactivate() {
}
