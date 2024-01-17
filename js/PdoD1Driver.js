Module.PdoD1Driver = Module.PdoD1Driver || (class PdoD1Driver
{
	prepare(db, query)
	{
		console.log('prepare', {db, query});
		return db.prepare(query);
	}

	doer(db, query)
	{
		console.log('doer', {db, query});
	}
});
