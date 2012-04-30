================
PHP Can HowTo
================

.. _installation:

Installation
------------

.. code-block:: bash

    $ /path/to/phpize
    $ ./configure --with-libevent=/path/to/libevent --with-php-config=/path/to/php-config
    $ make
    $ make install

Add "extension=can.so" to your php.ini file to load Can extension.


.. _tutorial-quickstart:

Mandatory Hello World example:

.. code-block:: php

    <?php

    use \Can\Server;
    use \Can\Server\Router;
    use \Can\Server\Route;
    use \Can\Server\Request;

    $router = new Router([
        new Route('/hello', 
            function(Request $request) 
                return 'Hello, World!';
            }
        )
    ]);

    (new Server('127.0.0.1', 4567))->start($router);

    ?>

This is it. Run this script, visit http://localhost:4567/hello and you will see "Hello, World!" in your browser. Here is how it works:

The class :php:class:`Route` binds a request handler to an URL path. In this case, we link the ``/hello`` URL to the closure as request handler,
add this route to the :php:class:`Router` instance and start the server with this router instance. You can define as many routes as you want. 
Whenever a browser requests an URL, the associated handler is called and the return value is sent back to the browser. Its as simple as that.

The :php:class:`Server` instance is instantiated with IP address and port number as parameters, and :php:meth:`Server::start()` starts the server. 
It runs on `localhost` port 4567 and serves requests until you hit ``Control-c``. 

Of course this is a very simple example, but it shows the basic concept of how HTTP services are built with Can. Continue reading and you'll see what else is possible.

.. _tutorial-routing:

Request routing
---------------

The Router distinguishes between two basic types of routes: static routes (e.g. ``/hello/world``) and dynamic routes (e.g. ``/hello/<name>``). 
A route that contains one or more wildcards it is considered dynamic. All other routes are static.
The simplest form of a wildcard consists of an identifier enclosed in angle brackets (e.g. ``<name>``). The identifier should be unique for a given route.
Each wildcard matches one or more characters, but stops at the first slash (/). This equals a regular expression of [^/]+ and ensures 
that only one path segment is matched and routes with more than one wildcard stay unambiguous. 

.. _tutorial-dynamic-routes:

Dynamic Routes
--------------

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

All wildcards passes the covered parts of the URL as associative array as second argument to the request callback. You can use them 
right away and implement RESTful, nice-looking and meaningful URLs with ease. Here are some other examples along with the URLs they'd match:

.. code-block:: php

    <?php
    
    $router = new Router([
        new Route(
            '/wiki/<file:path>',
            function(Request $request, $args) {
                return file_get_contents($args['file']);
            }
        ),
        new Route(
            '/user/<id:int>',
            function(Request $request, $args) {
                return User::get($args['id']);
            }
        )
    ]);
    
    ?>

.. _tutorial-request-methods:

HTTP Request Methods
--------------------

The HTTP protocol defines several request methods for different tasks. GET is the default for all routes 
with no other method specified. These routes will match GET requests only. To handle other methods such as POST, PUT or DELETE, 
add an appropriate class constant as 3. parameter to the Route constructor. You can use bitwise operators to combine multiple methods
for the same route.

.. code-block:: php

    <?php

    $router = new Router([
        new Route('/login', 
            function(Request $request) {
                return '<form method="POST">
                        Username: <input name="name" type="text" /><br />
                        Password: <input name="pass" type="password" /></br />
                        <input name="submit" type="submit" value="Log in" />
                        </form>';
            }, Route::METHOD_GET
        ),
        new Route('/login', 
            function(Request $request) {
                if (login($request->post['name'], $request->post['pass'])) {
                    return '<h2>You\'re ligged in!</h2>';
                }
                return '<h2>Login failed</h2>';
            }, Route::METHOD_POST
        )
    ]);

    ?>

In this example the ``/login`` URL is linked to two distinct callbacks, one for GET requests and another for 
POST requests. The first one displays a HTML form to the user. The second callback is invoked on a form 
submission and checks the login credentials the user entered into the form. The submited post data is available
in the :php:attr:`Request::$post` container.

.. _tutorial-routing-staticfiles:

Routing Static Files
--------------------

Static files such as images or CSS files are not served automatically. You have to add a route and a callback to 
control which files get served and where to find them:

.. code-block:: php

    <?php

    $router = new Router([
        new Route('/static/<filename>', 
            function(Request $request, $args) {
                $request->sendFile($args['filename'], '/path/to/your/static/files');
            }
        )
    ]);
    
    ?>
    
This example is limited to files directly within the ``/path/to/your/static/files`` directory because the ``<filename>`` 
wildcard won't match a path with a slash in it. To serve files in subdirectories, change the wildcard to use the `path` filter:

.. code-block:: php

    <?php

    $router = new Router([
        new Route('/static/<filename:path>', 
            function(Request $request, $args) {
                $request->sendFile($args['filename'], '/path/to/your/static/files');
            }
        )
    ]);

    ?>

The :php:meth:`Request::sendFile` method is a helper to serve files in a safe and convenient way. 
It automatically guesses a mime-type, adds a ``Last-Modified``  header, generate and add ETag header, restricts paths 
to a root directory for security reasons and generates appropriate error responses (401 on permission errors, 404 on missing files). 
It supports the ``If-Modified-Since`` and ``If-None-Match`` headers and eventually generates a 304 Not Modified response. 
You can pass a custom MIME type as 3. parameter to disable guessing:

.. code-block:: php

    <?php

    $router = new Router([
        new Route('/static/<filename:re:.*\.png>', 
            function(Request $request, $args) {
                $request->sendFile($args['filename'], 
                    '/path/to/your/static/files', 'image/png');
            }
        )
    ]);

    ?>
    
.. _tutorial-forced-download:
    
Forced Download
---------------

Most browsers try to open downloaded files if the MIME type is known and assigned to an application (e.g. PDF files). 
If this is not what you want, you can force a download dialog by setting 4. parameter to true:

.. code-block:: php

    <?php

    $router = new Router([
        new Route('/downloads/<filename:re:.*\.pdf>', 
            function(Request $request, $args) {
                $request->sendFile($args['filename'], 
                '/path/to/your/static/files', 'application/pdf', true);
            }
        )
    ]);

    ?>

.. _tutorial-output:

Generating content
------------------

Can supports the following range of types you can return from your request handler:

Strings
    Can returns strings as a whole and adds a ``Content-Length`` header based on the string length.
    
Empty Strings or ``Null``:
    These produce an empty output with the ``Content-Length`` header set to 0.
    
Objects
    If returned object implements JsonSerializable interface, return value of the object::jsonSerialize() will
    be set as output and ``Content-Type`` header will contain ``application/json``. 
    
All other types will produce 500 Internal Server Error

Uploading files
---------------

The request body of POST and PUT requests may contain form data encoded in various formats. 
The :php:attr:`Request::$post` container contains parsed textual form fields, :php:attr:`Request::$files` stores 
file upload informations.

Example:

.. code-block:: php

    <?php

    use \Can\Server;
    use \Can\Server\Router;
    use \Can\Server\Route;
    use \Can\Server\Request;

    $router = new Router([
        new Route('/upload', 
            function(Request $request) {
                switch ($request->method) {
                    case 'POST':
                        return '<pre>' . PHP_EOL . 
                               'post data: ' . print_r($request->post, true) . PHP_EOL .
                               'uploaded files: ' . print_r($request->files, true) . PHP_EOL;
                        break;
                    default:
                        return '
                            <form action="/upload" method="POST" enctype="multipart/form-data">
                            <input type="text" name="foo" value="bar"/><br/>
                            <input type="file" name="file1" /></br/>
                            <input type="text" name="baz" value="zak"/><br />
                            <input type="file" name="file2" /><br/>
                            <input type="submit" name="submit" value="Send"></form>
                        ';
                        break;
                }
            }, Route::METHOD_GET|Route::METHOD_POST
        )
    ]);

    (new Server('127.0.0.1', 4567))->start($router);

    ?>
    
Run this script, visit http://localhost:4567/upload, fill out and submit the form and you will see something similar:

.. code-block:: php

    post data: Array
    (
        [foo] => bar
        [baz] => zak
        [submit] => Send
    )

    uploaded files: Array
    (
        [0] => Array
            (
                [name] => file1
                [filename] => image1.jpg
                [filesize] => 32135
                [tmp_name] => /tmp/phpmcant7nl3iP
            )

        [1] => Array
            (
                [name] => file2
                [filename] => image2.jpg
                [filesize] => 5643
                [tmp_name] => /tmp/phpcanrHv051
            )

    )

Every item within :php:attr:`Request::$files` array contains uploaded file information: `name` contains the
form field name, `filename` - the real filename, `filesize` guess what?  and `tmp_name` is a path where uploaded file
content is stored. Please note that uploaded files (`tmp_name`) will be cleaned after :php:attr:`Request` object is destroyed 
therefor you must copy or move this files within request handler manually to be able to access it within your application.


To be continued...
------------------
