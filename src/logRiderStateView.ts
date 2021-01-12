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

  onStackNodeChanged(stackNode: StackNode | null) {
    if(stackNode === null) {
      this.currentStackNode = undefined;
      this.root = undefined;
      return;
    }

    let prevNode = this.currentStackNode;
    this.currentStackNode = stackNode;

    let caller = stackNode.caller; 

    this.root = LogNode.fromStackNode(stackNode, stackNode.line, prevNode?.line, );
    
    let thisPointersEncountered = new Set<string>();
    if(stackNode.loggedObject && 
      stackNode.loggedObject.thisAddress !== "") {
        thisPointersEncountered.add(stackNode.loggedObject.thisAddress);
      }
    

    while(caller) {
      let child = this.root;
      this.root = LogNode.fromStackNode(caller, stackNode.line, prevNode?.line, thisPointersEncountered);
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

  static fromStackNode(stackNode: StackNode, logLineIndex: number, prevlogLineIndex?: number, thisPointersEncountered?: Set<string>) {
    const nodeLabel = (stackNode.line + 1)+ " " + " " + stackNode.fileName + " :: " + stackNode.functionName + (stackNode.loggedObject ? " " + stackNode.loggedObject.thisAddress : "");
    let retNode = new LogNode(nodeLabel, vscode.TreeItemCollapsibleState.Expanded);

    if(stackNode.loggedObject) {
      if(!thisPointersEncountered || !thisPointersEncountered.has(stackNode.loggedObject.thisAddress)){
        stackNode.loggedObject?.pushedVariables.forEach((value: Map<number, string>, key: string) => {
      
          let val : string | null = null;
          for (const [k, v] of value.entries()) {
            if(logLineIndex >= k){
              val = v;
            } else {
              break;
            }
          }
          
          //TODO: this method has a bug.  If you jump into another block for a different "this pointer"
          // but far enough ahead that the value in your previous "this pointer" had changed, and then 
          // go back to the line in your "this pointer" where that change occured, it won't be highlighted.
          // There needs to be a way to cache all the values you've seen so far, and their latest variants.
          // this would require remembering the last line that you previously saw a this pointer.
          // ACTUALLY, could map it from the loggedObject!  Keep a secondary mapp where the loggedObject is the key (and/or give it
          // and id)
          let prevVal : string | null | undefined = undefined;
          if(prevlogLineIndex) {
            prevVal = null;
            for (const [k, v] of value.entries()) {
              if(prevlogLineIndex >= k){
                prevVal = v;
              } else {
                break;
              }
            }
          }    
    
          const paramLabel = key + ": " + val;
          const treeLabel : vscode.TreeItemLabel = {
            label: paramLabel,
            highlights: (val !== prevVal) ? [[0,paramLabel.length]] : undefined
          };
    
          retNode.children.push(new LogNode(treeLabel, vscode.TreeItemCollapsibleState.None));
        });
      }
    }

    return retNode;
  }

  constructor(label: string | vscode.TreeItemLabel, collapsibleState: vscode.TreeItemCollapsibleState){
    super(label, collapsibleState);
  }
}