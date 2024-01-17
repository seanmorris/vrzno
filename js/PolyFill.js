const FinReg = globalThis.FinalizationRegistry || class {
	register(){};
	unregister(){};
};

const wRef = globalThis.WeakRef || class{
	constructor(val){ this.val = val };
	deref() { return this.val };
};
