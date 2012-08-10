<?php

use \Can\Server\Request;
use \Can\Server\WebSocketRoute;
use \Can\Server\WebSocketConnection;
use \Can\HTTPError;
use \Exception;

class ChatWebSocketRoute extends WebSocketRoute
{
    /**
     * Connection to name map
     * @var array 
     */
    protected $map = array();
    
    /**
     * Container with active WebSocket connections
     * @var array 
     */
    protected $connections = array();
    
    /**
     * Check usage of the selected name and add connection
     * to the active connection container
     * 
     * @param Request $request          Request instance
     * @param array $args               Request arguments
     * @param WebSocketConnection $conn Current WebSocket connection
     * @throws HTTPError if selected name already exists
     */
    public function beforeHandshake(Request $request, array $args, WebSocketConnection $conn)
    {
        if (isset($this->map[$args['name']])) {
            throw new HTTPError(400, 'Name "' . $args['name'] . '" already in use');
        }
        $this->map[$args['name']] = $conn->id;
        $this->connections[$conn->id] = $conn;
    }
    
    /**
     * Notify all active connections about new connection
     * 
     * @param WebSocketConnection $conn Current WebSocket connection
     */
    public function afterHandshake(WebSocketConnection $conn)
    {
        $names = array_flip($this->map);
        $message = array(
            'type' => 'connect',
            'name' => $names[$conn->id],
            'id'   => $conn->id
        );
        $list = array();
        foreach ($this->connections as $activeConn) {
            if ($activeConn->id  != $conn->id) {
                $list[] = array('id' => $activeConn->id, 'name' => $names[$activeConn->id]);
            }
            $activeConn->send(json_encode($message));
        }
        $message = array(
            'type' => 'list',
            'data' => $list
        );
        $conn->send(json_encode($message));
    }
    
    /**
     * Publich incoming message to all active connections
     * 
     * @param type $payload JSON payload
     * @param WebSocketConnection $conn Current WebSocket connection
     * @throws Exception 
     */
    public function onMessage($payload, WebSocketConnection $conn)
    {
        $message = json_decode($payload);
        if (null === $message) {
            throw new Exception('Invalid payload');
        }
        $names = array_flip($this->map);
        switch ($message->type) {
            case 'msg':
                $message->id = $conn->id;
                $message->name = $names[$conn->id];
                foreach ($this->connections as $activeConn) {
                    $activeConn->send(json_encode($message));
                }
                break;
        }
    }
    
    /**
     * Remove closed connection from active connection container
     * and from the connection to name map, notify other active connstions
     * about it 
     * 
     * @param WebSocketConnection $conn Closed WebSocket connection
     */
    public function onClose(WebSocketConnection $conn)
    {
        $names = array_flip($this->map);
        $name  = $names[$conn->id];
        unset($this->map[$name]);
        if (isset($this->connections[$conn->id])) {
            unset($this->connections[$conn->id]);
            $message = array(
                'type' => 'disconnect',
                'id'   => $conn->id,
                'name' => $name
            );
            foreach ($this->connections as $activeConn) {
                $activeConn->send(json_encode($message));
            }
        }
    }
}