--TEST--
vrzno_test1() Basic test
--SKIPIF--
<?php
if (!extension_loaded('vrzno')) {
	echo 'skip';
}
?>
--FILE--
<?php
$ret = vrzno_test1();

var_dump($ret);
?>
--EXPECT--
The extension vrzno is loaded and working!
NULL
