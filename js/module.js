HEAP8 = new Int8Array(1);

Module.zvalMap = new WeakMap;

Module.fRegistry = Module.fRegistry || new FinReg(zvalPtr => {
	console.log('Garbage collecting! zVal@'+zvalPtr);
	Module.ccall(
		'vrzno_expose_dec_zrefcount'
		, 'number'
		, ['number']
		, [zvalPtr]
	);
});

Module.bufferMaps = new WeakMap;

Module.callables = Module.callables || new Module.WeakerMap();
Module.targets   = Module.targets   || new Module.UniqueIndex;

Module.PdoParams = new WeakMap;
Module.pdoDriver = Module.pdoDriver || new Module.PdoD1Driver;

Module.classes  = Module.classes || new WeakMap;
Module._classes = Module._classes || new Module.WeakerMap;

Module.targets.add(globalThis);
