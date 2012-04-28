.. _php-can-server:

===================
``\Can\Server`` API
===================

.. php:namespace:: Can

.. php:class:: Server
 
   HTTP Server class.
 
.. php:method:: __construct(string $ip, int $port, string $log_format = "", resource $log_handler = STDOUT)
 
  Constructor. Instantiates new HTTP server.
 
  :param string $ip: IP to bind server to.
  :param int $port:  The port to listen on.
  :param string $log_format: The log format string (ELF). 
  :param resource $log_handler: File descriptor for the logging, used only if $log_format defined, default STDOUT.
  :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
  :throws: * :php:class:`ServerBindingException` If binding of the server to given IP and port fails.
  
  Supported fields for ELF:
        - ``cs-uri`` - client-to-server requested URI
        - ``cs-query`` - client-to-server query string
        - ``c-ip`` - client IP address
        - ``c-port`` - client port number
        - ``cs-method`` - client-to-server HTTP Method 
        - ``sc-status`` - server-to-client HTTP Status code in the response 
        - ``sc-bytes`` - number of bytes sent 
        - ``time`` - time in 24-hour format, the machine's timezone  
        - ``date`` - date in yyyy-mm-dd format
        - ``time-taken`` - Time taken for transaction to complete in seconds 
        - ``x-memusage`` - memory usage
        - ``x-error`` - detailed error information if any
        
Example:
  
.. code-block:: php

    <?php
    
    use \Can\Server;
    
    // Bind server to all available network interfaces on port 8080 without logging
    $server = new Server('0.0.0.0', 8080);
    
    // Bind server to all available network interfaces on port 8080
    // with logging to standard output with defined format
    $server = new Server('0.0.0.0',  8080, 
        "x-reqnum time c-ip cs-method cs-uri sc-status " . 
            "sc-bytes time-taken x-memusage x-error\n"
    );

    // Bind server to all available network interfaces on port 8080
    // with logging to defined logfile and log format
    $server = new Server('0.0.0.0',  8080, 
        "x-reqnum time c-ip cs-method cs-uri sc-status " . 
            "sc-bytes time-taken x-memusage x-error\n",
        fopen('/tmp/phpcan.log', 'w')
    );
    
    ?>
 
.. php:method:: start(Router $router)
 
  Starts the server.
  
  :param Router $router: Instance of :php:class:`Router`.
  :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
 
.. php:method:: stop()
 
  Stops the server.
  
  :throws: * :php:class:`InvalidOperationException` If server is not running.
