--TEST--
\Can\Server\Router class tests
--SKIPIF--
<?php if(!extension_loaded("can")) print "skip"; ?>
--FILE--
<?php
use Can\Server\Route;
use Can\Server\Router;
try { $router = new Router('foo'); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $router = new Router(array('bar')); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $router = new Router(array(new Route('/', function () {}), 'bar')); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
$router = new Router();
try { $router->addRoute(); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $router->addRoute(false); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $router->addRoute('test'); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
unset($router);
$router = new Router(array(
    new Route('/', function ($request) {}),
    new Route('/<id:int>', function ($request) {}, Route::METHOD_GET|Route::METHOD_POST),
    new Route('/<id:float>', function ($request) {}, Route::METHOD_PUT|Route::METHOD_POST),
));
$router->addRoute(new Route('/<file:path>', function ($request) {}, Route::METHOD_DELETE|Route::METHOD_POST));
foreach ($router as $i => $route) {
    var_dump($route->getUri());
    var_dump($route->getUri(true));
    var_dump($route->getMethod());
}
$router->rewind();
while($router->valid()) {
    echo $router->key() . ' => ' . $router->current()->getUri() . PHP_EOL;
    $router->next();
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(1) "/"
bool(false)
int(1)
string(9) "/<id:int>"
string(18) "^/(?<id>-?[0-9]+)$"
int(3)
string(11) "/<id:float>"
string(19) "^/(?<id>-?[0-9.]+)$"
int(10)
string(12) "/<file:path>"
string(15) "^/(?<file>.+?)$"
int(18)
0 => /
1 => /<id:int>
2 => /<id:float>
3 => /<file:path>
