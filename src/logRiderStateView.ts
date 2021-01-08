import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';
import { StackNode } from './worldState';
import { stringify } from 'querystring';

export class LogRiderStateProvider implements vscode.TreeDataProvider<LogNode> {

  private _onDidChangeTreeData: vscode.EventEmitter<LogNode | undefined | null | void> = new vscode.EventEmitter<LogNode | undefined | null | void>();
	readonly onDidChangeTreeData: vscode.Event<LogNode | undefined | null | void> = this._onDidChangeTreeData.event;

  currentStackNode: StackNode | undefined;
  root: LogNode | undefined;
  
	constructor() {
  }
  
  getTreeItem(element: LogNode): vscode.TreeItem | Thenable<vscode.TreeItem> {
    return element;
  }
  getChildren(element?: LogNode): vscode.ProviderResult<LogNode[]> {
    if(this.root) {
      if(element === undefined) {
        return Promise.resolve([this.root]);
      }
      return Promise.resolve(element.children);
    }
  }

  onStackNodeChanged(stackNode: StackNode) {
    this.currentStackNode = stackNode;

    let caller = stackNode.caller; 

    this.root = LogNode.fromStackNode(stackNode);
    
    while(caller) {
      let child = this.root;
      this.root = LogNode.fromStackNode(caller);
      this.root.children.push(child);
      caller = caller.caller;
    }

    this.refresh();
  }

  refresh(): void {
    this._onDidChangeTreeData.fire();
  }


}
//console.log("changed node:" + stackNode.line + " depth:" + stackNode.depth + " thisAddress:" + stackNode.loggedObject?.thisAddress);
//
class LogNode extends vscode.TreeItem {
  children: LogNode[] = [];

  static fromStackNode(stackNode: StackNode) {
    const label = (stackNode.line + 1)+ " " + stackNode.loggedObject?.thisAddress + " " + stackNode.fileName + "::" + stackNode.functionName;
    let retNode = new LogNode(label, vscode.TreeItemCollapsibleState.Expanded);

    stackNode.loggedObject?.pushedVariables.forEach((value: Map<number, string>, key: string) => {
      
      let val : string | null = null;
      for (const [k, v] of value.entries()) {
        if(stackNode.line >= k){
          val = v;
        } else {
          break;
        }
      }

      retNode.children.push(new LogNode(key + ": " + val, vscode.TreeItemCollapsibleState.None));
    });

    return retNode;
  }

  constructor(label: string, collapsibleState: vscode.TreeItemCollapsibleState){
    super(label, collapsibleState);
  }
}