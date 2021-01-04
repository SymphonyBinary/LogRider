// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import { TextDecoder } from 'util';
import * as vscode from 'vscode';

class LoggedObject {
	thisAddress: string;
	pushedVariables: string[] = [];

	constructor(thisAddress: string) {
		this.thisAddress = thisAddress;
	}
}

class StackNode {
	line: number;
	depth: number;
	caller: StackNode | undefined;
	loggedObject: LoggedObject | undefined;

	constructor(line: number, depth: number, caller: StackNode | undefined, loggedObject: LoggedObject | undefined) {
		this.line = line;
		this.depth = depth;
		this.caller = caller;
		this.loggedObject = loggedObject;
	}
}

class WorldState {
	private _loggedObjects = new Map<string, LoggedObject>();
	private _lineData: (StackNode | undefined)[] = [];

	getOrCreateLoggedObject(thisAddress: string) : LoggedObject {
		let loggedObject = this._loggedObjects.get(thisAddress);
		if(loggedObject === undefined) {
			loggedObject = new LoggedObject(thisAddress);
			this._loggedObjects.set(thisAddress, loggedObject);
		}

		return loggedObject;
	}

	pushStackNode(node : StackNode | undefined) {
		this._lineData.push(node);
	}

	getStackNodeAt(lineNumber: number) {
		return this._lineData[lineNumber];
	}
}

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
			const doc = await vscode.workspace.openTextDocument(virtualDocURI); // calls back into the provider
			await vscode.window.showTextDocument(doc, { preview: false });
			provider.startUpdateLoop();
		}
	});

	context.subscriptions.push(disposable);
	
}

class LogRiderProvider implements vscode.TextDocumentContentProvider {
	// emitter and its event
	scheme = "logRider";
	onDidChangeEmitter = new vscode.EventEmitter<vscode.Uri>();
	onDidChange = this.onDidChangeEmitter.event;

	intervalHandle : NodeJS.Timeout | undefined;

	worldState = new WorldState();

	lastStackNode: StackNode | undefined;

	async provideTextDocumentContent(uri: vscode.Uri): Promise<string> {
		//TODO: this is tricky if we have more than one document open.  We need to keep a map
		// of all the logs open.


		//this gets us the entire uri, with the scheme too
		//return uri.toString();
		const filePath = vscode.Uri.parse(uri.fsPath);
		const encodedDoc = await vscode.workspace.fs.readFile(filePath);

		let dec = new TextDecoder("utf-8");
		let doc = dec.decode(encodedDoc);

		var re = /.*:: :  : (.*)/gi;
		doc = doc.replace(re, "$1");

		var lines = doc.split('\n');
		let currentStackNode = new Map<string, StackNode>();
		for(let i = 0; i < lines.length; i++) {
			const re = /(^[0-9]*) ([║╔╠╚]*).* ([0-9a-z]+)/;
			const match = lines[i].match(re);
			if(match === null) {
				//throw new Error("line is failing to match threadID and thisAddress: " + lines[i]).stack;
				this.worldState.pushStackNode(undefined);
				continue;
			}
			const threadID = match[1];
			const depth = match[2].length;
			const thisAddress = match[3];

			let loggedObject = this.worldState.getOrCreateLoggedObject(thisAddress);

			//const loggedObject = new LoggedObject(threadID, thisAddress);
			let caller = currentStackNode.get(threadID);
			if(caller !== undefined) {
				if(depth <= caller.depth) {
					caller = caller.caller;
				}
			} 

			let stackNode = new StackNode(i, depth, caller, loggedObject);
			this.worldState.pushStackNode(stackNode);
			currentStackNode.set(threadID, stackNode);
		}

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
			console.log("changed node:" + stackNode.line + " depth:" + stackNode.depth + " thisAddress:" + stackNode.loggedObject?.thisAddress);
			this.lastStackNode = stackNode;
		}
	}
}

// this method is called when your extension is deactivated
export function deactivate() {
}
