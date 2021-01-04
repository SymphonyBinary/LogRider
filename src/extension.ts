// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
import { TextDecoder } from 'util';
import * as vscode from 'vscode';

// this method is called when your extension is activated
// your extension is activated the very first time the command is executed
export function activate(context: vscode.ExtensionContext) {

	const scheme = "logRider";
	const provider = new LogRiderProvider();
	context.subscriptions.push(vscode.workspace.registerTextDocumentContentProvider(scheme, provider));

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
		}
	});

	context.subscriptions.push(disposable);
	
}

class LogRiderProvider implements vscode.TextDocumentContentProvider {
	// emitter and its event
	onDidChangeEmitter = new vscode.EventEmitter<vscode.Uri>();
	onDidChange = this.onDidChangeEmitter.event;

	async provideTextDocumentContent(uri: vscode.Uri): Promise<string> {
		//this gets us the entire uri, with the scheme too
		//return uri.toString();

		const filePath = vscode.Uri.parse(uri.fsPath);
		const encodedDoc = await vscode.workspace.fs.readFile(filePath);

		let dec = new TextDecoder("utf-8");
		let doc = dec.decode(encodedDoc);

		var re = /.*:: :  : (.*)/gi;
		doc = doc.replace(re, "$1");

		return doc.toString();
	}
}

// this method is called when your extension is deactivated
export function deactivate() {}
