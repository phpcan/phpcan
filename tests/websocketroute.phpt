--TEST--
\Can\Server\WebSocketRoute class tests
--SKIPIF--
<?php if(!extension_loaded("can")) print "skip"; ?>
--FILE--
<?php
function testHandshake($meth = 'GET', $headers = array(), $content = '') {
    $headers = array_merge(
            array(
                'Host' => 'localhost',
                'Upgrade' => 'websocket',
                'Connection' => 'Upgrade', 
                'Sec-WebSocket-Key' => 'dGhlIHNhbXBsZSBub25jZQ==', 
                'Origin' => 'http://localhost:45678', 
                'Sec-WebSocket-Protocol' => 'chat', 
                'Sec-WebSocket-Version' => '13'
            ), $headers
    );
    $str = '$s=new Can\Server("127.0.0.1", 45678);' . 
        '$s->start(new Can\Server\Router(array(' . 
        'new Can\Server\WebSocketRoute("/", function() {}),' . 
        'new Can\Server\Route("/quit", function() {global $s; $s->stop();exit;})' . 
        ')));';
    exec($_SERVER['_'] . " -r '" . $str . "' >/dev/null &");
    sleep(1);
    $fp = stream_socket_client("tcp://127.0.0.1:45678", $errno, $errstr, 30);
    if (!$fp) {
        echo "$errstr ($errno) 1\n";
    } else {
        $header = '';
        foreach ($headers as $k => $v) {
            $header .= $v !== null ? "$k: $v\r\n" : '';
        } 
        fwrite($fp, "$meth / HTTP/1.1\r\n" . $header . "\r\n" . $content);
        $response = '';
        while (!feof($fp) && $data = fgets($fp, 1024)) {
            if ($data == "\r\n") {
                break;
            }
            $response .= strpos($data, 'Date:') === false ? $data : '';
        }
        fclose($fp);
        $fp = stream_socket_client("tcp://127.0.0.1:45678", $errno, $errstr, 30);
        if (!$fp) {
            echo "$errstr ($errno)\n";
        } else {
            fwrite($fp, "GET /quit HTTP/1.0\r\n\r\n");
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
var_dump(testHandshake('HEAD'));
var_dump(testHandshake('GET', array('Upgrade' => null)));
var_dump(testHandshake('GET', array('Upgrade' => 'nada')));
var_dump(testHandshake('GET', array('Connection' => null)));
var_dump(testHandshake('GET', array('Connection' => 'close')));
var_dump(testHandshake('GET', array('Origin' => null)));
var_dump(testHandshake('GET', array('Sec-WebSocket-Key' => null)));
var_dump(testHandshake('GET', array('Sec-WebSocket-Version' => 99)));
var_dump(testHandshake('GET', array('Sec-WebSocket-Key' => null, 'Sec-WebSocket-Key1' => null)));
var_dump(testHandshake('GET', array('Sec-WebSocket-Key' => null, 'Sec-WebSocket-Key1' => 'x', 'Sec-WebSocket-Key2' => 'y')));
var_dump(testHandshake('GET', array('Sec-WebSocket-Key' => null, 
    'Sec-WebSocket-Key1' => '18x 6]8vM;54 *(5:  {   U1]8  z [  8', 
    'Sec-WebSocket-Key2' => '1_ tx7X d  <  nw  334J702) 7]o}` 0')));
var_dump(testHandshake('GET', array('Sec-WebSocket-Key' => null, 
    'Sec-WebSocket-Key1' => '18x 6]8vM;54 *(5:  {   U1]8  z [  8', 
    'Sec-WebSocket-Key2' => '1_ tx7X d  <  nw  334J702) 7]o}` 0'),
    "8jKS'y:G*Co,Wxa-"));
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
string(33) "HTTP/1.1 405 Method Not Allowed
"
string(90) "HTTP/1.1 400 Bad Request
Content-Length: 0
Content-Type: text/html; charset=ISO-8859-1
"
string(90) "HTTP/1.1 400 Bad Request
Content-Length: 0
Content-Type: text/html; charset=ISO-8859-1
"
string(90) "HTTP/1.1 400 Bad Request
Content-Length: 0
Content-Type: text/html; charset=ISO-8859-1
"
string(109) "HTTP/1.1 400 Bad Request
Content-Length: 0
Content-Type: text/html; charset=ISO-8859-1
Connection: close
"
string(90) "HTTP/1.1 400 Bad Request
Content-Length: 0
Content-Type: text/html; charset=ISO-8859-1
"
string(90) "HTTP/1.1 400 Bad Request
Content-Length: 0
Content-Type: text/html; charset=ISO-8859-1
"
string(90) "HTTP/1.1 400 Bad Request
Content-Length: 0
Content-Type: text/html; charset=ISO-8859-1
"
string(90) "HTTP/1.1 400 Bad Request
Content-Length: 0
Content-Type: text/html; charset=ISO-8859-1
"
string(90) "HTTP/1.1 400 Bad Request
Content-Length: 0
Content-Type: text/html; charset=ISO-8859-1
"
string(90) "HTTP/1.1 400 Bad Request
Content-Length: 0
Content-Type: text/html; charset=ISO-8859-1
"
string(201) "HTTP/1.1 101 WebSocket Protocol Handshake
Sec-WebSocket-Origin: http://localhost:45678
Sec-WebSocket-Location: ws://localhost/
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Protocol: chat
"
string(157) "HTTP/1.1 101 Switching Protocols
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Protocol: chat
"