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

.. php:method:: setData($data)

    Append some userdefined data to the connection. Any existing data will be overriden.

    :param mixed $data: Data to set.
    :throws: * :php:class:`InvalidParametersException` If invalid amount of parameters are passed to the method.


.. php:method:: getData()

    Get previously appended userdefined data from the connection.

    :returns: mixed data

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
            $conn->setData(array('name' => $args['name']));
        }
    }

    $route = new MyWebSocketRoute(
        '/<name>',
        function ($message, WebSocketConnection $conn) {
            $data = $conn->getData();
            $conn->send('Hello, ' . $data['name'] . ', your message was ' .  $message . '!');
        }
    );
    
    ?>


.. php:method:: send($message)

    Send message to the client.

    :param string $message: Message to send.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.

.. php:method:: close()

    Close this connection.

