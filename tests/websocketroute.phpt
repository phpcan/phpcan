--TEST--
\Can\Server\WebSocketRoute class tests
--SKIPIF--
<?php if(!extension_loaded("can")) print "skip"; ?>
--FILE--
<?php
function testHandshake($meth = 'GET') {
    $str = '$s=new Can\Server("127.0.0.1", 45678);' . 
        '$s->start(new Can\Server\Router(array(' . 
        'new Can\Server\WebSocketRoute("/", function() {}),' . 
        'new Can\Server\Route("/stop", function() {global $s; $s->stop();exit;})' . 
        ')));';
    exec ($_SERVER['_'] . " -r '" . $str . "' >/dev/null &");
    sleep(1);
    $fp = stream_socket_client("tcp://127.0.0.1:45678", $errno, $errstr, 30);
    if (!$fp) {
        echo "$errstr ($errno) 1\n";
    } else {
        $headers = "Upgrade: websocket\r\n" .
                   "Connection: Upgrade\r\n" . 
                   "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" . 
                   "Origin: http://localhost:45678\r\n" . 
                   "Sec-WebSocket-Protocol: chat\r\n" . 
                   "Sec-WebSocket-Version: 13\r\n";;
        fwrite($fp, "GET / HTTP/1.1\r\n$headers\r\n");
        $response = '';
        while (!feof($fp) && $data = fgets($fp, 1024)) {
            if ($data == "\r\n") {
                break;
            }
            $response .= $data;
        }
        fclose($fp);
        $fp = stream_socket_client("tcp://127.0.0.1:45678", $errno, $errstr, 30);
        if (!$fp) {
            echo "$errstr ($errno)\n";
        } else {
            fwrite($fp, "GET /stop HTTP/1.0\r\n\r\n");
            fclose($fp);
        }
        return $response;
    }
}
use Can\Server\WebSocketRoute;
try { $route = new WebSocketRoute(); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
try { $route = new WebSocketRoute('/', 'nada'); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidCallbackException); }
try { $route = new WebSocketRoute(false, function () {}); } catch (\Exception $e) { var_dump($e instanceof Can\InvalidParametersException); }
$route = new WebSocketRoute('/', function () {});
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
var_dump(testHandshake());
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
bool(true)
bool(true)
bool(true)
int(1)
string(5) "(GET)"
string(157) "HTTP/1.1 101 Switching Protocols
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Protocol: chat
"