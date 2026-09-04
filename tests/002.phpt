--TEST--
Legacy Vrzno helpers remain supported
--SKIPIF--
<?php
if (!extension_loaded('vrzno')) {
	echo 'skip';
}
?>
--FILE--
<?php
echo vrzno_eval('1 + 2'), "\n";
vrzno_eval('globalThis.__vrzno_phpt_sum = (left, right) => left + right');
echo vrzno_run('__vrzno_phpt_sum', [2, 3]), "\n";
?>
--EXPECT--
3
5
