--TEST--
\Can\Server\Route class tests
--SKIPIF--
<?php if(!extension_loaded("can")) print "skip"; ?>
--FILE--
<?php
use Can\Server\Route;
try { $route = new Route(); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $route = new Route('/', 'nada'); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidCallbackException); }
try { $route = new Route('/', function () {}, 'asd'); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $route = new Route('/', function () {}, false); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $route = new Route(false, function () {}, Route::METHOD_ALL); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
$route = new Route('/', function () {}, Route::METHOD_ALL);
try { $uri = $route->getUri(1); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $uri = $route->getUri(''); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $uri = $route->getUri(null); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
var_dump($uri = $route->getUri());
var_dump($uri = $route->getUri(true));
try { $uri = $route->getMethod(1); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $uri = $route->getMethod(''); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $uri = $route->getMethod(null); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
var_dump($uri = $route->getMethod());
var_dump($uri = $route->getMethod(true));
$route = new Route('/<id:int>', function ($request) {}, 
     Route::METHOD_GET|Route::METHOD_POST);
var_dump($route->getUri());
var_dump($route->getUri(true));
var_dump($route->getMethod());
var_dump($route->getMethod(true));
$route = new Route('/<id:float>', function ($request) {}, 
     Route::METHOD_PUT|Route::METHOD_POST);
var_dump($route->getUri());
var_dump($route->getUri(true));
var_dump($route->getMethod());
var_dump($route->getMethod(true));
$route = new Route('/<file:path>', function ($request) {}, 
     Route::METHOD_DELETE|Route::METHOD_POST);
var_dump($route->getUri());
var_dump($route->getUri(true));
var_dump($route->getMethod());
var_dump($route->getMethod(true));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(1) "/"
bool(false)
bool(true)
bool(true)
bool(true)
int(511)
string(54) "(GET|POST|HEAD|PUT|DELETE|OPTIONS|TRACE|CONNECT|PATCH)"
string(9) "/<id:int>"
string(18) "^/(?<id>-?[0-9]+)$"
int(3)
string(10) "(GET|POST)"
string(11) "/<id:float>"
string(19) "^/(?<id>-?[0-9.]+)$"
int(10)
string(10) "(POST|PUT)"
string(12) "/<file:path>"
string(15) "^/(?<file>.+?)$"
int(18)
string(13) "(POST|DELETE)"