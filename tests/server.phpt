--TEST--
\Can\Server class tests
--SKIPIF--
<?php if(!extension_loaded("can")) print "skip"; ?>
--FILE--
<?php
use Can\Server;
ini_set("date.timezone", "Europe/Berlin");
try { $s = new Server(); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $s = new Server(false); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $s = new Server(''); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $s = new Server('0.0.0.0'); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $s = new Server('0.0.0.0', false); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $s = new Server('0.0.0.0', -1); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
$s = new Server('0.0.0.0', 45678);
echo get_class($s) . PHP_EOL;
try { $s = new Server('0.0.0.0', 45678); } catch (\Exception $e) { var_dump($e instanceof Can\ServerBindingException); }
try { $s = new Server('0.0.0.0', 45678, false); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $s = new Server('0.0.0.0', 45678, ""); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $s = new Server('0.0.0.0', 45678, "x-error"); } catch (\Exception $e) { var_dump($e instanceof Can\ServerBindingException); }
try { $s = new Server('0.0.0.0', 45679, "x-error", false); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
$s = new Server('0.0.0.0', 45679, "x-error", fopen("/dev/null", "w"));
try { $s->stop(); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidOperationException); }
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Can\Server
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
