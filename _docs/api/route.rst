.. _php-can-route:

=========================
``\Can\Server\Route`` API
=========================

.. php:namespace:: Can\Server

.. php:class:: Route

   Route class.
   
.. php:method:: __construct(string $uri, callable $handler, int $methods = Route::METHOD_GET)

    Constructor. Instantiates new Server route.

    :param string $uri: Static or dynamic URI path.
    :param callable $handler: Request handler - any valid PHP callback
    :param int $methods: HTTP request methods bitmask this route associated with, default Route::METHOD_GET.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    
    The request handler must accepting :php:class:`Request` instance as first parameter and associative arguments array as second parameter
    in which you will find values of the wildcards defined within you URI path. See :ref:`tutorial-routing` for detailed information.
    
    
Example:
  
.. code-block:: php

    <?php

    use \Can\Server\Request;
    use \Can\Server\Route;  
    
    $route = new Route(
        '/hello/<name>',
        function (Request $request, array $args) {
            return 'Hello, ' . $args['name'] . '!';
        },
        Route::METHOD_GET
    );
    
    ?>

.. php:method:: handleRequest(Request $request, array $args)

    Default request handler. Will be invoked if no request handler will be paased to the
    constructor. By default this method throws :php:class:`InvalidCallbackException` with
    "Not implementd" error message. You must extend the Route class and override this method
    with request handler implementation.

    :param :php:class:`Request` $request: Request instance.
    :param array $args: associative arguments array with values of the wildcards defined within URI path.

Example:
  
.. code-block:: php

    <?php

    use \Can\Server\Request;
    use \Can\Server\Route;  
    
    class MyRoute extends Route
    {
        public function handleRequest(Request $request, array $args)
        {
            return 'Hello, ' . $args['name'] . '!';
        }
    }

    $route = new MyRoute('/hello/<name>');
    
    ?>
    
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
    
.. php:method:: setMethod(int $methods)

    Set HTTP method this route applies to.
    
    :param int $methods: HTTP request methods bitmask this route associated with.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.

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

