HEAP8 = new Int8Array(1);

Module.zvalMap  = new WeakMap;
Module._zvalMap = Module._zvalMap || new Module.WeakerMap;

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

Module.classes  = Module.classes  || new WeakMap;
Module._classes = Module._classes || new Module.WeakerMap;

Module.PdoParams = new WeakMap;
Module.pdoDriver = Module.pdoDriver || new Module.PdoD1Driver;

Module.targets.add(globalThis);

Module.onRefresh.add(() => {
	console.log('VRZNO refresh callback');
	Module.callables.clear();
	Module.targets.clear();
	Module._classes.clear();
	Module._zvalMap.clear();
	Module.targets.byObject.set(globalThis, 1);
	Module.targets.byInteger.set(1, globalThis);
});
