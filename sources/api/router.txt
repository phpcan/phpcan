.. _php-can-router:

==========================
``\Can\Server\Router`` API
==========================

.. php:namespace:: Can\Server

.. php:class:: Router

   Router class. Since Router class implements Iterator you can use ``foreach`` to iterate through Router instances.
    
.. php:method:: __construct(array $routes)

   Constructor. Instantiates new Server router.
   
   :param array $routes: array where each element is an instance of :php:class:`Route`.
   
   :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
   
.. php:method:: addRoute(Route $route)

   Adds new route to the router.
   
   :param Route $route: Instance of :php:class:`Route` to add.
   
   :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
   
Example:
  
.. code-block:: php

    <?php
    
    use \Can\Server\Router;
    use \Can\Server\Route;  
    
    $handler = function(){};
    
    $router = new Router(
        array(
            new Route('/', $handler),
            new Route('/home', $handler, Route::METHOD_GET|Route::METHOD_PUT),
            new Route('/<file:path>', $handler, Route::METHOD_GET|Route::METHOD_HEAD),
        )
    );
    
    $router->addRoute(
        new Route('/what/ever', $handler)
    );
    
    foreach ($router as $route) {
        echo 'uri = "' . $route->getUri() . '" methods: ' . 
	     $route->getMethod() . ' ' . $route->getMethod(true) . PHP_EOL;
    }
    
    ?>
