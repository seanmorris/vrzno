--TEST--
Check Vrzno extension metadata
--SKIPIF--
<?php
if (!extension_loaded('vrzno')) {
	echo 'skip';
}
?>
--FILE--
<?php
echo phpversion('vrzno'), "\n";
echo class_exists('Vrzno') ? "class\n" : "missing\n";
echo (new ReflectionFunction('vrzno_timeout'))->getReturnType(), "\n";
?>
--EXPECT--
0.2.0
class
void
