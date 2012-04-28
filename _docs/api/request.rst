.. _php-can-request:

===========================
``\Can\Server\Request`` API
===========================

.. php:namespace:: Can\Server

.. php:class:: Request

   Request class
   
.. php:attr:: method

    ``string`` HTTP request method
    
.. php:attr:: uri 

    ``string`` URI path.
    
.. php:attr:: query 

    ``string`` The query string.
    
.. php:attr:: protocol 

    ``string`` HTTP protocol used by client to communicate with the web server.
    
.. php:attr:: remote_addr 

    ``string`` Client's remote IP address.
    
.. php:attr:: remote_port 

    ``integer`` The port being used by client to communicate with the web server.
    
.. php:attr:: headers

    ``array`` Container with HTTP request headers 
    
.. php:attr:: cookies

    ``array`` Container with parsed cookies sent by the client
    
.. php:attr:: get 

    ``array`` Container with parsed GET parameters
    
.. php:attr:: post 

    ``array`` Container with parsed POST parameters

.. php:attr:: files 

    ``array`` Container with uploaded files information
    
.. php:attr:: time 

    ``float`` The timestamp of the start of the request.
    
    


    

    
