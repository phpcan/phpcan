=====================
Buddel: Web framework
=====================

Copyright: (c) 2012 Dmitry Vinogradov


Buddel is a fast and simple web framework for writing a lightweight stateless or stateful PHP HTTP services based on libevent (http://libevent.org).
This project is inspired by Bottle (Python Web Framework: http://bottlepy.org)


Installation
============

    ./configure --with-libevent=LIBEVENT-PATH --with-php-config=PHP_CONFIG_PATH
    make
    make install

Add "extension=buddel.so" to your php.ini file to load Buddel classes.

Mandatory Hello World example:

    <?php

    use \Buddel\Server;
    use \Buddel\Server\Router;
    use \Buddel\Server\Route;
    use \Buddel\Server\Request;

    $server = new Server('127.0.0.1', 4567);

    $router = new Router([
        new Route('/hello/world', 
            function(Request $request) 
                return 'Hello, World';
            }
        )
    ]);

    $server->start($router);

    ?>

Execute this script with your PHP cli binary (requires PHP 5.4+) and point your browser to http://localhost:4567/hello/world

The most parts of the following documentation is based on BottlePy documentation adapted for Buddel PHP extension.

Request routing
===============

The Router distinguishes between two basic types of routes: static routes (e.g. ``/hello/world``) and dynamic routes (e.g. ``/hello/<name>``). 
A route that contains one or more wildcards it is considered dynamic. All other routes are static.
The simplest form of a wildcard consists of an identifier enclosed in angle brackets (e.g. ``<name>``). The identifier should be unique for a given route.
Each wildcard matches one or more characters, but stops at the first slash (/). This equals a regular expression of [^/]+ and ensures 
that only one path segment is matched and routes with more than one wildcard stay unambiguous. 

Dynamic Routes
==============

Routes that contain wildcards are called dynamic routes (as opposed to static routes) and match more than one URL at the same time. 
For example, the route ``/hello/<name>`` accepts requests for ``/hello/alice`` as well as ``/hello/bob``, but not for ``/hello``, ``/hello/`` or ``/hello/mr/smith``.

Filters are used to define more specific wildcards, and/or transform the covered part of the URL before it is passed to the callback. 
A filtered wildcard is declared as ``<name:filter>`` or ``<name:filter:config>``. The syntax for the optional config part depends 
on the filter used.

The following filters are implemented by default and more may be added:

* **:int** matches (signed) digits only and converts the value to integer.
* **:float** similar to :int but for decimal numbers.
* **:path** matches all characters including the slash character in a non-greedy way and can be used to match more than one path segment.
* **:re** allows you to specify a custom regular expression in the config field. The matched value is not modified.

Each wildcard passes the covered part of the URL as a keyword argument to the request callback. You can use them right away and 
implement RESTful, nice-looking and meaningful URLs with ease. Here are some other examples along with the URLs they’d match:

    <?php

    $router = new Router([
        new Route('/wiki/<pagename>', 'getWiki'),           // matches /wiki/imprint
        new Route('/<action>/<user>', 'User::doAction'),    // matches /follow/foobar
        new Route('/user/<id:int>',   'User::get'),         // matches /user/123
    ]);

    ?>

HTTP Request Methods
====================

The HTTP protocol defines several request methods for different tasks. GET is the default for all routes 
with no other method specified. These routes will match GET requests only. To handle other methods such as POST, PUT or DELETE, 
add an appropriate class constant as 3. parameter to the Route constructor. You can use bitwise operators to combine multiple methods
for the same route.

    <?php

    $router = new Router([
        new Route('/login', 
            function(Request $request) {
                return '<form method="POST">
                        Username: <input name="name" type="text" /><br />
                        Password: <input name="pass" type="password" /></br />
                        <input name="submit" type="submit" value="Log in" />
                        </form>';
            }
        ),
        new Route('/login', 
            function(Request $request) {
                if (login($request->post['name'], $request->post['pass'])) {
                    return '<h2>You\'re ligged in!</h2>';
                }
                return '<h2>Login failed</h2>';
            }, 
            Route::METHOD_POST
        )
    ]);

    ?>

To be continued...
==================
