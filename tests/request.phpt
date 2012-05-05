--TEST--
\Can\Server class tests
--SKIPIF--
<?php if(!extension_loaded("can")) print "skip"; ?>
--FILE--
<?php
function test($code, $expected, $meth = 'GET')
{
    $context = stream_context_create(array('http'=>array('method'=>$meth,"header"=>"X-My-Custom-Header: WOW\r\nContent-Type:text/plain\r\n", "content"=>"foobar")));
    $str = '$s=new Can\Server("0.0.0.0", 45678, "x-error\n");$s->start(new Can\Server\Router([new ' . 
       'Can\Server\Route("/<uri>",function($r, $a) {global $s;if (strpos($a["uri"]' . 
       ',"quit")===0) $s->stop(); else try{if ($a["uri"] === "r")return $r->get["o"];%s;}catch(\Exception $e){return get_class($e).' . 
       '":".$e->getMessage();}},Can\Server\Route::METHOD_ALL)]));';
    $cmd = sprintf($str, $code);
    exec ($_SERVER['_'] . " -r '" . $cmd . "' >/dev/null &");
    sleep(1);
    $r = file_get_contents('http://127.0.0.1:45678/test', false, $context);
    var_dump($r === $expected);
    if ($r !== $expected) {
        var_dump($r);
        var_dump($expected);
    }
    @file_get_contents('http://127.0.0.1:45678/quit', false, $context);
}

test('return $r->findRequestHeader(false);', 'Can\InvalidParametersException:Can\Server\RequestContext::findRequestHeader(string $header)');
test('return $r->findRequestHeader(null);', 'Can\InvalidParametersException:Can\Server\RequestContext::findRequestHeader(string $header)');
test('return $r->findRequestHeader("x-mY-custOm-HeadER");', 'WOW');
test('return var_export($r->findRequestHeader("nada"),1);', 'false');
test('return var_export($r->findRequestHeader("nada"),1);', 'false');
test('return var_export($r->getRequestBody(),1);', 'false');
test('return var_export($r->getRequestBody(),1);', '\'foobar\'', 'PUT');
test('return $r->addResponseHeader();', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::addResponseHeader(string $header, string $value)');
test('return $r->addResponseHeader(false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::addResponseHeader(string $header, string $value)');
test('return $r->addResponseHeader(null, false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::addResponseHeader(string $header, string $value)');
test('return var_export($r->addResponseHeader("foo", ""),1);', 'true');
test('return var_export($r->addResponseHeader("foo", "bar"),1);', 'true');
test('return $r->removeResponseHeader();', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::removeResponseHeader(string $header[, string $value])');
test('return $r->removeResponseHeader(false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::removeResponseHeader(string $header[, string $value])');
test('return $r->removeResponseHeader(false, null);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::removeResponseHeader(string $header[, string $value])');
test('return var_export($r->removeResponseHeader("foo"), 1);', 'false');
test('$r->addResponseHeader("foo", "bar");return var_export($r->removeResponseHeader("foo"), 1);', 'true');
test('$r->addResponseHeader("foo", "bar");return var_export($r->removeResponseHeader("foo","baz"), 1);', 'false');
test('$r->addResponseHeader("foo", "bar");return var_export($r->removeResponseHeader("foo","bar"), 1);', 'true');
test('return json_encode($r->getResponseHeaders());', '[]');
test('$r->addResponseHeader("foo", "bar");return json_encode($r->getResponseHeaders(), 1);', '{"foo":"bar"}');
test('return $r->setResponseStatus();', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setResponseStatus(int $status)');
test('return $r->setResponseStatus(false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setResponseStatus(int $status)');
test('return $r->setResponseStatus(1);', 'Can\\InvalidParametersException:Unexpected HTTP status, expecting range is 100-599');
test('return $r->setResponseStatus(600);', 'Can\\InvalidParametersException:Unexpected HTTP status, expecting range is 100-599');
test('$r->setResponseStatus(201);return (string)$r->response_status;', '201');
test('return $r->redirect();', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::redirect(string $location[, int $status = 302])');
test('return $r->redirect(false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::redirect(string $location[, int $status = 302])');
test('return $r->redirect("");', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::redirect(string $location[, int $status = 302])');
test('return $r->redirect("/foo/bar", false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::redirect(string $location[, int $status = 302])');
test('return var_export($r->redirect("/r?o=true"),1);', 'true');
test('return $r->redirect("/r?o=true");', 'true');
test('return $r->redirect("/r?o=true", 301);', 'true');
test('$r->setCookie();', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie(false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("");', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=deleted; expires=Thu, 01-Jan-1970 00:00:01 GMT"}');
test('$r->setCookie("foo", "bar");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "bar", false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", -1);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 1);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar; expires=Thu, 01-Jan-1970 00:00:01 GMT"}');
test('$r->setCookie("foo", "bar", 0, false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 0, "");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "bar", 0, "/");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar; path=\/"}');
test('$r->setCookie("foo", "bar", 0, "", false);', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 0, "", "");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "bar", 0, "", "www.google.de");return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar; domain=www.google.de"}');
test('$r->setCookie("foo", "bar", 0, "", "", "");', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 0, "", "", false);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "bar", 0, "", "", true);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar; secure"}');
test('$r->setCookie("foo", "bar", 0, "", "", false, "");', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 0, "", "", false, false);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "bar", 0, "", "", false, true);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar; httponly"}');
test('$r->setCookie("foo", "bar", 0, "", "", false, false, "");', 'Can\\InvalidParametersException:Can\\Server\\RequestContext::setCookie(string $name [, string $value [, int $expire = 0 [, string $path [, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])');
test('$r->setCookie("foo", "bar", 0, "", "", false, false, false);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=bar"}');
test('$r->setCookie("foo", "=,; \t\r\n\013\014", 0, "", "", false, false, false);', 'Can\\InvalidParametersException:Cookie values cannot contain any of the following characters \'=,; \t\r\n\013\014\'');
test('$r->setCookie("foo", "=,; \t\r\n\013\014", 0, "", "", false, false, true);return json_encode($r->getResponseHeaders());', '{"Set-Cookie":"foo=%3D%2C%3B+%09%0D%0A%0B%0C"}');
test('return $r->sendFile();', 'Can\InvalidParametersException:Can\Server\RequestContext::sendFile(string $filename[, string $root[, string $mimetype[, string $download[, int $chunksize=10240]]]])');
test('return $r->sendFile(false);', 'Can\InvalidParametersException:Can\Server\RequestContext::sendFile(string $filename[, string $root[, string $mimetype[, string $download[, int $chunksize=10240]]]])');
test('return $r->sendFile("");', 'Can\InvalidParametersException:Can\Server\RequestContext::sendFile(string $filename[, string $root[, string $mimetype[, string $download[, int $chunksize=10240]]]])');
test('return $r->sendFile("/tmp");', 'Can\HTTPError:Requested path \'/tmp\' is a directory');
test('return $r->sendFile("../../");', 'Can\HTTPError:Bogus file requested \'../../\'');
test('return $r->sendFile("foobar", "/tmp");', 'Can\HTTPError:Requested file \'/tmp/foobar\' does not exist');
test('return $r->sendFile("../../../passwd\0.htm", "/tmp");', 'Can\InvalidParametersException:Can\Server\RequestContext::sendFile(string $filename[, string $root[, string $mimetype[, string $download[, int $chunksize=10240]]]])');
test('file_put_contents("/tmp/test.txt", "foobar");return $r->sendFile("/test.txt", "/tmp");', 'foobar');
test('file_put_contents("/tmp/test.txt", "foobar", "application/json");return $r->sendFile("/test.txt", "/tmp");', 'foobar');
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
