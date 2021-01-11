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

		//var re = /.*:: :  : (.*)/gi;
	//	var re = /.*C_LOG : (.*?) (.*?) (.* \[.*\]::\[.*\]::\[)(.*)(\] [0-9a-z]+)/gi;
		const re = /.*C_LOG : (.*?) (.*?) (.*) ([0-9a-z]+)/gi;
		doc = doc.replace(re, (match: string, sub1: string, sub2: string, sub3: string, sub4: string) : string => {
			sub2 = sub2.replace(/:/g, "║");
			sub2 = sub2.replace("F", "╔");
			sub2 = sub2.replace("L", "╚");
			sub2 = sub2.replace("-", "╠");
			sub2 = sub2.replace(">", "╾");
			if(sub4 !== "0") {
				const reLineInfo = /(.* \[.*\]::\[.*\]::\[)(.*)(\])/i;
				sub3 = sub3.replace(reLineInfo, (matchSub3: string, sub31: string, sub32: string, sub33: string) : string => {
					sub32 = sub32.replace(/.*[ :]+([a-zA-Z0-9_]*::[a-zA-Z0-9_]*).*/g, "$1()");
					return sub31 + sub32 + sub33;
				});
			}
			//(.*::.+\(.*)
			//var 
			return sub1 + " " + sub2 + " " + sub3 + " " + sub4;
		});

		var lines = doc.split('\n');
		let currentStackNode = new Map<string, StackNode>();
		let failureStackNode = new StackNode(0,0,"","",undefined, undefined);
		for(let i = 0; i < lines.length; i++) {
			const re = /(^[0-9]*) ([║╔╠╚]*) (.*) \[(.*)\]::\[(.*)\]::\[(.*)\] ([0-9a-z]+)/;
			const match = lines[i].match(re);
			if(match !== null) {
				const threadID = match[1];
				const depth = match[2].length;
				const functionId = match[3];
				const lineNumber = match[4];
				const filename = match[5];
				const functionName = match[6];
				const thisAddress = match[7];

				let loggedObject = this.worldState.getOrCreateLoggedObject(thisAddress);

				//const loggedObject = new LoggedObject(threadID, thisAddress);
				let caller = currentStackNode.get(threadID);
				if(caller !== undefined) {
					if(Math.abs(depth - caller.depth) > 1) {
						console.error("unexpected jump in depth on line " + (i+1));
						vscode.window.showErrorMessage("unexpected jump in depth on line " + (i+1));
					}

					for(; caller && (depth - 1) < caller.depth; caller = caller.caller) {
						
					}
				}

				let stackNode = new StackNode(i, depth, filename, functionName, caller, loggedObject);
				this.worldState.pushStackNode(stackNode);
				currentStackNode.set(threadID, stackNode);
				failureStackNode = stackNode;
				continue;
			} 
			
			const commandRe = /(^[0-9]*) ([║╠╚╾]*) (.*) \[(.*)\] (.*): (.*)/;
			const commandMatch = lines[i].match(commandRe);
			if(commandMatch){
				const threadID = commandMatch[1];
				const depth = commandMatch[2].length;
				const functionId = commandMatch[3];
				const lineNumber = commandMatch[4];
				const command = commandMatch[5];
				const payload = commandMatch[6];

				let caller = currentStackNode.get(threadID);

				if(caller !== undefined) {
					if(Math.abs(depth - caller.depth) > 1) {
						console.error("unexpected jump in depth on line " + (i+1));
						vscode.window.showErrorMessage("unexpected jump in depth on line " + (i+1));
					}

					for(; caller && (depth - 1) < caller.depth; caller = caller.caller) {
						
					}
				}

				if(caller === undefined) {
					throw new Error("push found without caller");
				}

				if(command === "SET") {
					const setCommandRe = /(.*) = (.*)/;
					const setCommandMatch = payload.match(setCommandRe);
					if(setCommandMatch) {
						const varName = setCommandMatch[1];
						const varValue = setCommandMatch[2]; 
						let maybeCurrent = caller.loggedObject?.pushedVariables.get(varName);
						if(maybeCurrent) {
							maybeCurrent.set(i, varValue);
						} else {
							caller.loggedObject?.pushedVariables.set(varName, new Map([[i, varValue]]));
						}
					} else {
						console.error("unrecognized SET command");
					}
				}
				
				//fallback if nothing matches
				let stackNode = new StackNode(i, caller.depth, caller.fileName, caller.functionName, caller.caller, caller.loggedObject);
				this.worldState.pushStackNode(stackNode);
				currentStackNode.set(threadID, stackNode);
				failureStackNode = stackNode;
				continue;
			}

			console.error("unknown line");

			let stackNode = new StackNode(i, failureStackNode.depth, failureStackNode.fileName, failureStackNode.functionName, failureStackNode.caller, failureStackNode.loggedObject);
			this.worldState.pushStackNode(stackNode);
		}

		this.loaded = true;

		return doc.toString();
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
