.. _php-can-websocket-route:

==================================
``\Can\Server\WebSocketRoute`` API
==================================

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

.. php:method:: beforeHandshake(Request $request, array $args, WebSocketConnection $conn)

    This method will be invoked before WebSocket handshake will be sent to the client. By default this method do
    nothing. You must extend WebSocketRoute class to override this method with your logic, e.g. examine Request 
    headers or values of the wildcards defined within you URI path, add additional response headers or decline
    the connection by throwing :php:class:`HTTPError` exception. Although WebSocketConnection instance already
    exists the real WebSocket connection is not established yet, so it makes no sense to call WebSocketConnection::send()
    or WebSocketConnection::close() at this time.

    :param Request $request: :php:class:`Request` instance.
    :param array $args: associative arguments array with values of the wildcards defined within URI path.
    :param WebSocketConnection $conn: Instance of :php:class:`WebSocketConnection`, WebSocket connection associated with this request.

Example:
  
.. code-block:: php

    <?php

    use \Can\Server\Request;
    use \Can\Server\WebSocketRoute;
    use \Can\Server\WebSocketConnection;
    use \Can\HTTPError;
    
    class MyWebSocketRoute extends WebSocketRoute
    {
        public function beforeHandshake(Request $request, array $args, WebSocketConnection $conn)
        {
            if ($args['name'] == 'foo') {
                $request->addResponseHeader('Foo', 'Bar');
            } else {
                throw new HTTPError(400, 'Expecting foo..');
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

.. php:method:: afterHandshake(WebSocketConnection $conn)

    This method will be invoked after WebSocket handshake sent to the client. By default this method do
    nothing. You must extend WebSocketRoute class to override this method with your logic. At this time
    the WebSocket connection already established, so you can send a message or close this connection.

    :param WebSocketConnection $conn: Instance of :php:class:`WebSocketConnection`, WebSocket connection.

Example:
  
.. code-block:: php

    <?php

    use \Can\Server\Request;
    use \Can\Server\WebSocketRoute;
    use \Can\Server\WebSocketConnection;
    use \Can\HTTPError;
    
    class MyWebSocketRoute extends WebSocketRoute
    {
        public function afterHandshake(WebSocketConnection $conn)
        {
            $conn->send('Welcome, dude');
        }
    }

    $route = new MyWebSocketRoute(
        '/<name>',
        function ($message, WebSocketConnection $conn) {
            $conn->send('Hello, your message was ' .  $message . '!');
        }
    );
    
    ?>

.. php:method:: onMessage($message, WebSocketConnection $conn)

    This method will be invoked on incoming messages from WebSocket connection $conn. Use given
    WebSocketConnection to send a message to the client.

    :param string $message: Incoming WebSocket message.
    :param WebSocketConnection $conn: Instance of :php:class:`WebSocketConnection`, WebSocket connection.

Example:
  
.. code-block:: php

    <?php

    use \Can\Server\Request;
    use \Can\Server\WebSocketRoute;
    use \Can\Server\WebSocketConnection;
    use \Can\HTTPError;
    
    class MyWebSocketRoute extends WebSocketRoute
    {
        public function onMessage($message, WebSocketConnection $conn)
        {
            $conn->send('Hello, your message was ' .  $message . '!');
        }
    }

    $route = new MyWebSocketRoute('/<name>');
    
    ?>


.. php:method:: onClose(\Can\Server\WebSocketConnection $conn)

    This method will be invoked if WebSocket connection $conn is closed. 

    :param WebSocketConnection $conn: Instance of :php:class:`WebSocketConnection`, WebSocket connection.

