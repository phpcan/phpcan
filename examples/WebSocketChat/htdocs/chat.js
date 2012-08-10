$(function () {
    "use strict";
    var content = $('#content');
    var members = $('#members');
    var input   = $('#input');
    var label   = $('#label');
    var button  = $('#button');
    var ws      = null;
    var name    = null;

    window.WebSocket = window.WebSocket || window.MozWebSocket;

    if (!window.WebSocket) {
        content.html($('<p>', {text: 'Sorry, but your browser doesn\'t support WebSockets.'} ));
        input.hide();
        $('span').hide();
        return;
    }
    
    button.click(function(e) {
        switch (button.text()) {
            case 'Connect':
                name = input.val();
                if (input.val() == '') {
                    name = 'Guest';
                }
                ws = new WebSocket('ws://127.0.0.1:4567/chat/' + name);
                ws.onopen    = function() {
                    content.prepend('<p><span style="color:green">You are now as ' + 
                        name + ' connected</span></p>');
                    button.text('Disconnect');
                    label.text('Write your message:');
                    input.val('');
                };
                ws.onerror   = function() {
                    content.html($('<p>', {text: 'Sorry, but there\'s some problem with your '
                                                + 'connection or the server is down.</p>'} ));
                };
                ws.onclose  = function()  {
                    content.prepend('<p><span style="color:red">You are disconnected</span></p>');
                    members.empty();
                    button.text('Connect');
                    label.text('Choose your name:');
                    input.val(name);
                };
                ws.onmessage = function(evt) {
                    var json = JSON.parse(evt.data);
                    var dt = new Date();
                    switch (json.type) {
                        case 'msg':
                            content.prepend('<p><span style="font-weight:' + (json.name == name ? 'bold' : 'normal') + '">' + json.name + '</span> @ ' +
                                + (dt.getHours() < 10 ? '0' + dt.getHours() : dt.getHours()) + ':'
                                + (dt.getMinutes() < 10 ? '0' + dt.getMinutes() : dt.getMinutes())
                                + ': ' + json.text + '</p>');
                            break;
                        case 'list':
                            for (var i = 0; i < json.data.length; i++) {
                                members.append('<p id="' + json.data[i].name + '"><span style="font-weight:' + (json.data[i].name == name ? 'bold' : 'normal') + 
                                    '">' + json.data[i].name + '</span>' + '</p>');
                            }
                            
                            break;
                        case 'connect':
                            members.append('<p id="' + json.name + '"><span style="font-weight:' + (json.name == name ? 'bold' : 'normal') + 
                                    '">' + json.name + '</span>' + '</p>');
                            break;
                        case 'disconnect':
                            content.prepend('<p><span style="color:grey"><i>' + json.name + ' disconnected</i></span></p>');
                            $('#'+json.name).detach();
                            break;
                    }
                };
                break;
            case 'Disconnect':
                ws.close();
                break;
        }
    });

    input.keydown(function(e) {
        if (e.keyCode === 13) {
            var msg = $(this).val();
            if (!msg) {
                return;
            }
            var payload = '{"type":"msg", "text":"' + msg + '"}';
            ws.send(payload);
            $(this).val('');
        }
    });

});