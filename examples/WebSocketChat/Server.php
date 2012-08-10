<?php

use \Can\Server;
use \Can\Server\Router;
use \Can\Server\Route;
use \Can\Server\WebSocketRoute;
use \Can\Server\WebSocketConnection;
use \Can\Server\Request;

ini_set("date.timezone", "Europe/Berlin");

require_once __DIR__ . '/ChatWebSocketRoute.php';

$server = new Server('0.0.0.0', 4567, 
    "time c-ip cs-method cs-uri sc-status sc-bytes time-taken x-memusage x-error\n");
$server->start(
    new Router(
        array(
            new ChatWebSocketRoute('/chat/<name>'),
            new Route(
                '/<file:path>',
                function(Request $request, array $args)
                {
                    $request->sendFile($args['file'], __DIR__ . '/htdocs/');
                }
            )
        )
    )
);