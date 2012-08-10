.. _php-can-websocket-route:

=========================
``\Can\Server\WebSocketRoute`` API
=========================

.. php:namespace:: Can\Server

.. php:class:: WebSocketRoute

   WebSocketRoute class.
   
.. php:method:: __construct(string $uri, callable $handler)

    Constructor. Instantiates new Server WebSocket route.

    :param string $uri: Static or dynamic URI path.
    :param callable $handler: onMessage handler - any valid PHP callback
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    
    The onMessage handler must accepting ``string`` message as first parameter and :php:class:`WebSocketConnection` instance as second parameter
    which you can use to communicate with the WebSocket client. See :ref:`tutorial-websockets` for detailed information.
    
    
Example:
  
.. code-block:: php

    <?php

    use \Can\Server\Request;
    use \Can\Server\WebSocketRoute;
    use \Can\Server\WebSocketConnection;
    
    $route = new WebSocketRoute(
        '/hello',
        function ($message, WebSocketConnection $conn) {
            $conn->send('Hello, your message was ' .  $message . '!');
        }
    );
    
    ?>

.. php:method:: onHandshake(\Can\Server\Request $request, array $args, \Can\Server\WebSocketConnection $conn)

    This method will be invoked before WebSocket handshake will be sent to the client. By default this method do
    nothing. You must extend WebSocketRoute class to override this method with your logic, e.g. examine Request 
    headers or values of the wildcards defined within you URI path, add additional response headers or simply close
    the connection by calling $conn->close().

Example:
  
.. code-block:: php

    <?php

    use \Can\Server\Request;
    use \Can\Server\WebSocketRoute;
    use \Can\Server\WebSocketConnection;
    
    class MyWebSocketRoute extends WebSocketRoute
    {
        public function onHandshake(Request $request, array $args, WebSocketConnection $conn)
        {
            if ($args['name'] == 'foo') {
                $request->addResponseHeader('Foo', 'Bar');
            } else {
                $conn->close();
            }
        }
    }

    $route = new MyWebSocketRoute(
        '/<name>',
        function ($message, WebSocketConnection $conn) {
            $conn->send('Hello, your message was ' .  $message . '!');
        }
    );
    
    ?>

.. php:method:: onMessage($message, \Can\Server\WebSocketConnection $conn)

    This method will be invoked on incoming messages from WebSocket connection $conn. By default this method do
    nothing. You must extend WebSocketRoute class to override this method with your logic, e.g. examine Request 
    headers or values of the wildcards defined within you URI path, add additional response headers or simply close
    the connection by calling $conn->close().

    
.. php:method:: getUri(bool $as_regexp = false)

    Get URI path associated with this Route instance.
    
    :param bool $as_regexp: If set to ``true``, return value is a valid PCRE representation of the URI path.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    :returns: string
    
.. php:method:: getMethod(bool $as_regexp = false)

    Get HTTP request method associated with this Route instance.
    
    :param bool $as_regexp: If set to ``true``, return value is a valid PCRE representation of the method(s).
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    :returns: string
    
.. php:method:: getHandler()

    Get request handler associated with this Route instance.
    
    :returns: callable
    
.. php:const:: METHOD_GET
.. php:const:: METHOD_POST
.. php:const:: METHOD_PUT
.. php:const:: METHOD_DELETE
.. php:const:: METHOD_HEAD
.. php:const:: METHOD_OPTIONS
.. php:const:: METHOD_TRACE
.. php:const:: METHOD_CONNECT
.. php:const:: METHOD_PATCH

