

$(function() { // Esto asegura que el código se ejecute una vez que el DOM esté listo

    const esp32_ip = "192.168.1.100"; // ¡CAMBIA ESTO POR LA IP DE TU ESP32!

    // Elementos de la perilla y texto (AHORA USANDO JQUERY)
    // Ya no necesitamos 'speedDirectionKnob' ni 'knobIndicator' como variables separadas
    // ya que jQuery Knob maneja el input directamente
    const speedDirectionKnobInput = $('#speedDirectionKnobInput'); // Seleccionamos el input
    const speedValueSpan = $('#speedValue');
    const currentDirectionSpan = $('#currentDirection');

    // Elementos existentes (AHORA USANDO JQUERY)
    const stepModeSwitch = $('#stepModeSwitch');
    const turnOnBtn = $('#turnOnBtn');
    const turnOffBtn = $('#turnOffBtn');
    const angleInput = $('#angleInput');
    const rotateAngleBtn = $('#rotateAngleBtn');
    const fullTurnBtn = $('#fullTurnBtn');
    const motorStaticImage = $('#motorStaticImage'); // Esto también es un elemento de imagen

    let currentDirection = "detenido";
    let currentSpeedPercentage = 0;
    let motorEnabled = false;

    // Función para enviar comandos al ESP32 (ya estaba bien)
    async function sendCommand(command, value = '') {
        try {
            const url = `http://${esp32_ip}/control?cmd=${command}&val=${value}`;
            console.log(`Enviando: ${url}`);
            const response = await fetch(url);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            const data = await response.text();
            console.log('Respuesta del ESP32:', data);
            updateStatus();
        } catch (error) {
            console.error('Error al enviar comando al ESP32:', error);
            $('#connectionStatus').text('Desconectado').removeClass('text-success').addClass('text-danger');
        }
    }

    // --- Lógica para la Perilla de Velocidad y Dirección (con jQuery Knob) ---
    speedDirectionKnobInput.knob({ // Usamos la variable jQuery
        'change': function (v) {
            let knobValue = parseInt(v);

            if (knobValue > 0) {
                currentDirection = "derecha";
                currentSpeedPercentage = knobValue;
            } else if (knobValue < 0) {
                currentDirection = "izquierda";
                currentSpeedPercentage = Math.abs(knobValue);
            } else {
                currentDirection = "detenido";
                currentSpeedPercentage = 0;
            }

            updateKnobDisplay();

            if (motorEnabled) {
                sendCommand('speed', currentSpeedPercentage);
                sendCommand('direction', currentDirection === "detenido" ? "stop" : currentDirection);
            }
        },
        'release': function (v) {
            if (currentSpeedPercentage === 0) {
                if (motorEnabled) {
                    sendCommand('speed', 0);
                    sendCommand('direction', 'stop');
                }
            }
        },
        'width': 200,
        'height': 200,
        'min': -100,
        'max': 100,
        'angleOffset': -135,
        'angleArc': 270,
        'fgColor': "#28a745",
        'bgColor': "#495057",
        'skin': "tron",
        'cursor': true,
        'displayInput': false
    });

    function updateKnobDisplay() {
        speedValueSpan.text(`${currentSpeedPercentage}%`);
        currentDirectionSpan.text(currentDirection.charAt(0).toUpperCase() + currentDirection.slice(1));
    }
    // --- FIN Lógica para la Perilla ---


    // Control de encendido/apagado del motor
    turnOnBtn.on('click', () => {
        sendCommand('enable');
        motorEnabled = true;
        motorStaticImage.attr('src', "motor_moving.gif");
    });

    turnOffBtn.on('click', () => {
        sendCommand('disable');
        motorEnabled = false;
        motorStaticImage.attr('src', "motor_static.png");
        // Al apagar, también detener la perilla y resetear estados
        speedDirectionKnobInput.val(0).trigger('change'); // Usa la variable jQuery
        currentDirection = "detenido";
        currentSpeedPercentage = 0;
        updateKnobDisplay();
    });

    // Control de modo de pasos
    stepModeSwitch.on('change', function() {
        const mode = $(this).prop('checked') ? 'half' : 'full';
        sendCommand('step_mode', mode);
        $(this).next('label').text($(this).prop('checked') ? 'Medios Pasos' : 'Pasos Completos');
    });

    // Control de giro por ángulo
    rotateAngleBtn.on('click', () => {
        const angle = angleInput.val();
        if (angle >= 0 && angle <= 360) {
            sendCommand('rotate_angle', angle);
        } else {
            alert('Por favor, ingresa un ángulo entre 0 y 360 grados.');
        }
    });

    // Vuelta completa (360°)
    fullTurnBtn.on('click', () => {
        sendCommand('rotate_angle', '360');
    });

    // Función para actualizar el estado del ESP32
    async function updateStatus() {
        try {
            const response = await fetch(`http://${esp32_ip}/status`);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            const status = await response.json();
            $('#currentStep').text(status.currentStep);
            $('#connectionStatus').text('Conectado').removeClass('text-danger').addClass('text-success');

            if (status.motorEnabled && motorStaticImage.attr('src').includes('motor_static.png')) {
                motorStaticImage.attr('src', "motor_moving.gif");
            } else if (!status.motorEnabled && motorStaticImage.attr('src').includes('motor_moving.gif')) {
                motorStaticImage.attr('src', "motor_static.png");
            }

        } catch (error) {
            console.error('Error al obtener estado del ESP32:', error);
            $('#connectionStatus').text('Desconectado').removeClass('text-success').addClass('text-danger');
        }
    }

    // Actualizar el estado cada 2 segundos
    setInterval(updateStatus, 2000);
    updateStatus(); // Llamar al inicio para mostrar el estado inicial

    // Inicializar la visualización al cargar
    updateKnobDisplay();

}); // Fin de $(function() { ...