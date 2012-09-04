--TEST--
\Can\Server class tests
--SKIPIF--
<?php if(!extension_loaded("can")) print "skip"; ?>
--FILE--
<?php
function test($code, $expected, $meth = 'GET', $rHdrs = null, $headers = '')
{
    $str = '$s=new Can\Server("127.0.0.1", 45678);' . 
           '$s->start(new Can\Server\Router(array(' . 
           'new Can\Server\Route("/<uri>",' . 
           'function($r, $a) {' . 
           'global $s;' . 
           'if (strpos($a["uri"],"quit")===0) $s->stop(); ' . 
           'else try {%s;} catch(\Exception $e){$m=get_class($e).":".$e->getMessage();' . 
           'if ($r->status == Can\Server\Request::STATUS_SENDING) {' . 
           '$r->sendResponseChunk($m);} else return $m;}}' . 
           ',Can\Server\Route::METHOD_ALL))));';
    $cmd = sprintf($str, $code);
    exec ($_SERVER['_'] . " -r '" . $cmd . "' >/dev/null &");
    sleep(1);
    
    $fp = stream_socket_client("tcp://127.0.0.1:45678", $errno, $errstr, 30);
    if (!$fp) {
        echo "$errstr ($errno)\n";
    } else {
        $headers .= "Content-Type: text/plain\r\n";
        $content = '';
        switch($meth) {
            case 'POST':
            case 'PUT':
                $content = "foobar";
                $headers .= "Content-Length: " . strlen($content) . "\r\n";
                break;
        }
        fwrite($fp, $meth . " /test HTTP/1.0\r\n$headers\r\n" . $content);
        $r = '';
        $is_body = false;
        $_headers = array();
        while (!feof($fp) && $data = fgets($fp, 1024)) {
            if ($data == "\r\n") {
                $is_body = true;
                continue;
            }
            if ($is_body) $r .= $data;
            else {
                if (false === strpos($data, ':')) continue;
                list($header, $value) = explode(':', $data);
                if ('' !== ($v = trim($value))) $_headers[strtolower(trim($header))] = $v;
            }
        }
        fclose($fp);
        if (is_array($rHdrs) && !empty($rHdrs)) {
            $counter = count($rHdrs);
            $matched = 0;
            foreach ($rHdrs as $key => $val) {
                $k = strtolower($key);
                if (isset($_headers[$k]) && strtolower($_headers[$k]) === strtolower($val)) {
                    $matched++;
                }
            }
            var_dump($matched === $counter);
            if ($matched !== $counter) {
                var_dump($_headers);
                var_dump($rHdrs);
            }/**/
        }
        if ($expected !== null) {
            var_dump($r === $expected);
            if ($r !== $expected) {
                var_dump($code);
                var_dump($r);
                var_dump($expected);
            }/**/
        }
        $fp = stream_socket_client("tcp://127.0.0.1:45678", $errno, $errstr, 30);
        if (!$fp) {
            echo "$errstr ($errno)\n";
        } else {
            fwrite($fp, "GET /quit HTTP/1.0\r\n$headers\r\n");
            $r = '';
            while (!feof($fp)) {
                $r .= fgets($fp, 1024);
            }
            fclose($fp);
        }
        
    }
}
/**/
test('$r->responseCode = 500;', "Can\InvalidOperationException:Cannot update readonly property Can\Server\Request::\$responseCode");
test('return $r->findRequestHeader(false);', "Can\InvalidParametersException:Can\Server\Request::findRequestHeader(string \$header)");
test('return $r->findRequestHeader(null);', "Can\InvalidParametersException:Can\Server\Request::findRequestHeader(string \$header)");
test('return $r->findRequestHeader("x-mY-custOm-HeadER");', "WOW", "GET", null, "X-My-Custom-Header: WOW\r\n");
test('return var_export($r->findRequestHeader("nada"),1);', "false");
test('return var_export($r->findRequestHeader("nada"),1);', "false");
test('return var_export($r->getRequestBody(),1);', "false");
test('return var_export($r->getRequestBody(),1);', '\'foobar\'', 'PUT');
test('return $r->addResponseHeader();', 'Can\\InvalidParametersException:Can\\Server\\Request::addResponseHeader(string $header, string $value)');
test('return $r->addResponseHeader(false);', 'Can\\InvalidParametersException:Can\\Server\\Request::addResponseHeader(string $header, string $value)');
test('return $r->addResponseHeader(null, false);', 'Can\\InvalidParametersException:Can\\Server\\Request::addResponseHeader(string $header, string $value)');
test('return var_export($r->addResponseHeader("foo", ""),1);', 'true');
test('return var_export($r->addResponseHeader("foo", "bar"),1);', 'true');
test('return $r->removeResponseHeader();', 'Can\\InvalidParametersException:Can\\Server\\Request::removeResponseHeader(string $header[, string $value])');
test('return $r->removeResponseHeader(false);', 'Can\\InvalidParametersException:Can\\Server\\Request::removeResponseHeader(string $header[, string $value])');
test('return $r->removeResponseHeader(false, null);', 'Can\\InvalidParametersException:Can\\Server\\Request::removeResponseHeader(string $header[, string $value])');
test('return var_export($r->removeResponseHeader("foo"), 1);', 'false');
test('$r->addResponseHeader("foo", "bar");return var_export($r->removeResponseHeader("foo"), 1);', 'true');
test('$r->addResponseHeader("foo", "bar");return var_export($r->removeResponseHeader("foo","baz"), 1);', 'false');
test('$r->addResponseHeader("foo", "bar");return var_export($r->removeResponseHeader("foo","bar"), 1);', 'true');
test('return json_encode($r->getResponseHeaders());', '[]');
test('$r->addResponseHeader("foo", "bar");return json_encode($r->getResponseHeaders(), 1);', '{"foo":"bar"}');
test('return $r->setResponseStatus();', 'Can\\InvalidParametersException:Can\\Server\\Request::setResponseStatus(int $status)');
test('return $r->setResponseStatus(false);', 'Can\\InvalidParametersException:Can\\Server\\Request::setResponseStatus(int $status)');
test('return $r->setResponseStatus(1);', 'Can\\InvalidParametersException:Unexpected HTTP status, expecting range is 100-599');
test('return $r->setResponseStatus(600);', 'Can\\InvalidParametersException:Unexpected HTTP status, expecting range is 100-599');
test('$r->setResponseStatus(201);return (string)$r->responseCode;', '201');
test('return $r->redirect();', 'Can\\InvalidParametersException:Can\\Server\\Request::redirect(string $location[, int $status = 302])');
test('return $r->redirect(false);', 'Can\\InvalidParametersException:Can\\Server\\Request::redirect(string $location[, int $status = 302])');
test('return $r->redirect("");', 'Can\\InvalidParametersException:Can\\Server\\Request::redirect(string $location[, int $status = 302])');
test('return $r->redirect("/foo/bar", false);', 'Can\\InvalidParametersException:Can\\Server\\Request::redirect(string $location[, int $status = 302])');
test('return var_export($r->redirect("/r?o=true"),1);', null, 'GET', array('Location' => '/r?o=true'));
test('return $r->redirect("/r?o=true");', null, 'GET', array('Location' => '/r?o=true'));
test('return $r->redirect("/r?o=true", 301);', null, 'GET', array('Location' => '/r?o=true'));
test('$r->setCookie();', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie(false);', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("");', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", false);', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=deleted; expires=Thu, 01-Jan-1970 00:00:01 GMT"}');
test('$r->setCookie("foo", "bar");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "bar", false);', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", -1);', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 1);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar; expires=Thu, 01-Jan-1970 00:00:01 GMT"}');
test('$r->setCookie("foo", "bar", 0, false);', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 0, "");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "bar", 0, "/");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar; path=\/"}');
test('$r->setCookie("foo", "bar", 0, "", false);', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 0, "", "");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "bar", 0, "", "www.google.de");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar; domain=www.google.de"}');
test('$r->setCookie("foo", "bar", 0, "", "", "");', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 0, "", "", false);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "bar", 0, "", "", true);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar; secure"}');
test('$r->setCookie("foo", "bar", 0, "", "", false, "");', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 0, "", "", false, false);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "bar", 0, "", "", false, true);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar; httponly"}');
test('$r->setCookie("foo", "bar", 0, "", "", false, false, "");', 'Can\\InvalidParametersException:Can\\Server\\Request::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 0, "", "", false, false, false);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "=,; \t\r\n\013\014", 0, "", "", false, false, false);', 'Can\\InvalidParametersException:Cookie values cannot contain any of the following characters \'=,; \t\r\n\013\014\'');
test('$r->setCookie("foo", "=,; \t\r\n\013\014", 0, "", "", false, false, true);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=%3D%2C%3B+%09%0D%0A%0B%0C"}');
test('return $r->sendFile();', 'Can\InvalidParametersException:Can\Server\Request::sendFile(string $filename[, string $root[, string $mimetype[, string $download[, int $chunksize=10240]]]])');
test('return $r->sendFile(false);', 'Can\InvalidParametersException:Can\Server\Request::sendFile(string $filename[, string $root[, string $mimetype[, string $download[, int $chunksize=10240]]]])');
test('return $r->sendFile("");', 'Can\InvalidParametersException:Can\Server\Request::sendFile(string $filename[, string $root[, string $mimetype[, string $download[, int $chunksize=10240]]]])');
test('return $r->sendFile("/tmp");', 'Can\HTTPError:Requested path \'/tmp\' is a directory');
test('return $r->sendFile("../../");', 'Can\HTTPError:Bogus file requested \'../../\'');
test('return $r->sendFile("foobar", "/tmp");', 'Can\HTTPError:Requested file \'/tmp/foobar\' does not exist');
test('return $r->sendFile("../../../passwd\0.htm", "/tmp");', 'Can\InvalidParametersException:Can\Server\Request::sendFile(string $filename[, string $root[, string $mimetype[, string $download[, int $chunksize=10240]]]])');
test('file_put_contents("/tmp/test.txt", "foobar");return $r->sendFile("/test.txt", "/tmp");', 'foobar');
test('file_put_contents("/tmp/test.txt", "foobar", "application/json");return $r->sendFile("/test.txt", "/tmp");', 'foobar');
test('return $r->sendResponseStart();', 'Can\\InvalidParametersException:Can\\Server\\Request::sendResponseStart(int $status[, string $reason])');
test('return $r->sendResponseStart(false);', 'Can\\InvalidParametersException:Can\\Server\\Request::sendResponseStart(int $status[, string $reason])');
test('return $r->sendResponseStart("123");', 'Can\\InvalidParametersException:Can\\Server\\Request::sendResponseStart(int $status[, string $reason])');
test('return $r->sendResponseStart(200, false);', 'Can\\InvalidParametersException:Can\\Server\\Request::sendResponseStart(int $status[, string $reason])');
test('return $r->sendResponseStart(200, "");', 'Can\\InvalidParametersException:Can\\Server\\Request::sendResponseStart(int $status[, string $reason])');
test('return $r->sendResponseStart(99);', 'Can\\InvalidParametersException:Unexpected HTTP status, expecting range is 100-599');
test('$r->sendResponseStart(200);', "");
test('$r->sendResponseChunk();', 'Can\\InvalidOperationException:Invalid status');
test('$r->sendResponseStart(200);$r->sendResponseChunk();', "Can\InvalidParametersException:Can\Server\Request::sendResponseChunk(string \$chunk)");
test('$r->sendResponseStart(200);$r->sendResponseChunk(false);', "Can\InvalidParametersException:Can\Server\Request::sendResponseChunk(string \$chunk)");
test('$r->sendResponseStart(200);$r->sendResponseChunk(1234);', "Can\InvalidParametersException:Can\Server\Request::sendResponseChunk(string \$chunk)");
test('$r->sendResponseStart(200);$r->sendResponseChunk("");', "");
test('$r->sendResponseStart(200);$r->sendResponseChunk("foobar");$r->sendResponseEnd();', "foobar");
test('return array(1,2,3,4);', "");
test('$r->addResponseHeader("Content-Type","application/json");return array(1,2,3,4);', "[1,2,3,4]");
if (PHP_MINOR_VERSION < 4) {
    test('$r->addResponseHeader("Content-Type","application/json");return array(1,2,3,4);', "[1,2,3,4]");
} else {
    test('class a implements JsonSerializable {public function jsonSerialize() {return array(1,2,3,4);}} return new a;', "[1,2,3,4]");
}
test('file_put_contents(__DIR__ . "/test.txt", "qwertzuiopasdfghjklyxcvbnm");return $r->sendFile("test.txt", __DIR__);', "qwertzuiopasdfghjklyxcvbnm");
test('file_put_contents(__DIR__ . "/test.txt", "qwertzuiopasdfghjklyxcvbnm");return $r->sendFile("test.txt", __DIR__);', "wertz", "GET", null, "Range: bytes=1-5\r\n");
test('file_put_contents(__DIR__ . "/test.txt", "qwertzuiopasdfghjklyxcvbnm");return $r->sendFile("test.txt", __DIR__);', "xcvbnm", "GET", null, "Range: bytes=-6\r\n");
test('file_put_contents(__DIR__ . "/test.txt", "qwertzuiopasdfghjklyxcvbnm");return $r->sendFile("test.txt", __DIR__);', "asdfghjklyxcvbnm", "GET", null, "Range: bytes=10-\r\n");
test('unlink(__DIR__ . "/test.txt");"";', "");
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
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
