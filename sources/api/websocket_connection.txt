.. _php-can-websocket-connection:

=======================================
``\Can\Server\WebSocketConnection`` API
=======================================

.. php:namespace:: Can\Server

.. php:class:: WebSocketConnection

   WebSocketConnection class used to communicate with the WebSocket client. This class is final and cannot be 
   instantiated manually.

.. php:attr:: id

    ``string`` Unique identifier of the connection

.. php:method:: setTimeout($timeout)

    Set timeout for this connection in seconds.

    :param int $timeout: Timeout in seconds.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.

Example:
  
.. code-block:: php

    <?php

    use \Can\Server\Request;
    use \Can\Server\WebSocketRoute;
    use \Can\Server\WebSocketConnection;
    
    class MyWebSocketRoute extends WebSocketRoute
    {
        public function beforeHandshake(Request $request, array $args, WebSocketConnection $conn)
        {
            $conn->setTimeout(300);
        }
    }

    $route = new MyWebSocketRoute(
        '/<name>',
        function ($message, WebSocketConnection $conn) {
            $conn->send('Hello, your message was ' .  $message . '!');
        }
    );
    
    ?>

.. php:method:: send($message)

    Send message to the client.

    :param string $message: Message to send.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.

.. php:method:: close()

    Close this connection.

