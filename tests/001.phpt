--TEST--
Check if vrzno is loaded
--SKIPIF--
<?php
if (!extension_loaded('vrzno')) {
	echo 'skip';
}
?>
--FILE--
<?php
echo 'The extension "vrzno" is available';
?>
--EXPECT--
The extension "vrzno" is available
