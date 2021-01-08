export class LoggedObject {
	thisAddress: string;
	pushedVariables = new Map<string, Map<number, string>>();

	constructor(thisAddress: string) {
		this.thisAddress = thisAddress;
	}
}

export class StackNode {
	line: number;
  depth: number;
  fileName: string;
  functionName: string;
	caller: StackNode | undefined;
	loggedObject: LoggedObject | undefined;

  constructor(
      line: number, 
      depth: number, 
      fileName: string,
      functionName: string,
      caller: StackNode | undefined, 
      loggedObject: LoggedObject | undefined) { //maybe this shouldn't be allowed to be undefined
		this.line = line;
    this.depth = depth;
    this.fileName = fileName;
    this.functionName = functionName;
		this.caller = caller;
		this.loggedObject = loggedObject;
	}
}

export class WorldState {
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