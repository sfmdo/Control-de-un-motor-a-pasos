document.addEventListener('DOMContentLoaded', () => {
    // URL base de tu ESP32
    const esp32IP = 'http://192.168.0.183'; // ¡¡¡CAMBIA ESTA IP POR LA DE TU ESP32!!!

    // --- Elementos de la UI ---
    const connectionStatus = document.getElementById('connectionStatus');
    const currentSteps = document.getElementById('currentSteps');
    const motorDirection = document.getElementById('motorDirection');
    const stepModeDisplay = document.getElementById('stepMode');
    const refreshStatusBtn = document.getElementById('refreshStatusBtn');

    const motorOnBtn = document.getElementById('motorOnBtn');
    const motorOffBtn = document.getElementById('motorOffBtn');
    const powerIndicator = document.getElementById('powerIndicator');

    const motorAnimation = document.getElementById('motorAnimation');
    const directionArrow = document.getElementById('directionArrow');

    const speedDirectionKnob = document.getElementById('speedDirectionKnob');
    const knobSpeedValue = document.getElementById('knobSpeedValue');
    const knobDirectionValue = document.getElementById('knobDirectionValue');
    const sendKnobDataBtn = document.getElementById('sendKnobDataBtn');

    const halfStepModeBtn = document.getElementById('halfStepModeBtn');
    const fullStepModeBtn = document.getElementById('fullStepModeBtn');

    const angleInput = document.getElementById('angleInput');
    const rotateByAngleBtn = document.getElementById('rotateByAngleBtn');
    const fullRevolutionBtn = document.getElementById('fullRevolutionBtn');
    const halfRevolutionBtn = document.getElementById('halfRevolutionBtn');

    const oscillateBtn = document.getElementById('oscillateBtn');
    const toggleTorqueBtn = document.getElementById('toggleTorqueBtn');
    const returnToZeroBtn = document.getElementById('returnToZeroBtn');

    // --- Variables de Estado Local ---
    let isMotorOn = false;
    let currentSpeedPercentage = 0; // 0-100%
    let currentDirectionAngle = 0;  // 0-360° (0° = arriba, 90° = derecha)
    let isKnobDragging = false;
    let currentMotorDirection = 'Nula'; // Para la animación de la flecha
    let currentMotorSpeed = 0; // Velocidad real del motor (para la animación)
    let torqueState = true; // El estado del torque (se obtiene del ESP32)

    // --- Funciones de Comunicación con ESP32 ---
    async function sendCommand(cmd, val = '') {
        try {
            const url = `${esp32IP}/control?cmd=${cmd}&val=${val}`;
            console.log(`Enviando: ${url}`);
            const response = await fetch(url, { mode: 'cors' });
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            const data = await response.text();
            console.log(`Respuesta ESP32: ${data}`);
            updateStatus(); // Actualizar estado después de cada comando
        } catch (error) {
            console.error('Error al enviar comando al ESP32:', error);
            connectionStatus.textContent = 'Error';
            connectionStatus.classList.remove('bg-success');
            connectionStatus.classList.add('bg-danger');
        }
    }

    async function updateStatus() {
        try {
            const url = `${esp32IP}/status`;
            const response = await fetch(url, { mode: 'cors' });
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            const status = await response.json();
            console.log('Estado recibido:', status);

            connectionStatus.textContent = 'Conectado';
            connectionStatus.classList.remove('bg-danger');
            connectionStatus.classList.add('bg-success');

            currentSteps.textContent = status.currentStep;
            motorDirection.textContent = status.direction === 'right' ? 'Derecha (CW)' : 'Izquierda (CCW)';
            currentMotorDirection = status.direction; // Actualiza para la animación

            stepModeDisplay.textContent = status.stepMode === 'half' ? 'Medios Pasos' : 'Pasos Completos';

            // Actualizar estado del foco
            isMotorOn = status.motorEnabled;
            if (isMotorOn) {
                powerIndicator.classList.add('on');
                motorOnBtn.classList.add('active');
                motorOffBtn.classList.remove('active');
            } else {
                powerIndicator.classList.remove('on');
                motorOnBtn.classList.remove('active');
                motorOffBtn.classList.add('active');
                // Si el motor está apagado, la animación debe parar si no hay torque
                if (!status.torqueEnabled) {
                     stopMotorAnimation();
                }
            }

            // Actualizar estado del torque
            torqueState = status.torqueEnabled;
            toggleTorqueBtn.textContent = torqueState ? 'Toggle Torque (Habilitado)' : 'Toggle Torque (Deshabilitado)';
            if (torqueState) {
                toggleTorqueBtn.classList.remove('btn-outline-light');
                toggleTorqueBtn.classList.add('btn-light');
            } else {
                toggleTorqueBtn.classList.add('btn-outline-light');
                toggleTorqueBtn.classList.remove('btn-light');
            }


            // Mapeo inverso de velocidad del ESP32 a porcentaje de la UI
            // ESP32: 4000us (0%) a 1000us (100%)
            currentMotorSpeed = status.speed; // La velocidad ya viene como porcentaje
            updateMotorAnimation(); // Actualiza animación con la velocidad y dirección

            // Actualizar botones de modo de pasos
            if (status.stepMode === 'half') {
                halfStepModeBtn.classList.add('active');
                fullStepModeBtn.classList.remove('active');
            } else {
                fullStepModeBtn.classList.add('active');
                halfStepModeBtn.classList.remove('active');
            }

        } catch (error) {
            console.error('Error al obtener estado del ESP32:', error);
            connectionStatus.textContent = 'Desconectado';
            connectionStatus.classList.remove('bg-success');
            connectionStatus.classList.add('bg-danger');
            // Si hay un error de conexión, el motor se considera apagado
            isMotorOn = false;
            powerIndicator.classList.remove('on');
            motorOnBtn.classList.remove('active');
            motorOffBtn.classList.add('active');
            stopMotorAnimation();
        }
    }

    // --- Funciones Auxiliares ---
    function map(value, inMin, inMax, outMin, outMax) {
        return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
    }

    // --- Control del Knob (Velocidad y Dirección) ---
    function updateKnob(angle) {
        // Normalizar ángulo para que 0 sea hacia arriba
        let normalizedAngle = angle;
        if (normalizedAngle < 0) {
            normalizedAngle += 360;
        }

        let speedPercentage;
        let directionLabel;

        // La perilla va de 0 a 360.
        // 0° (arriba) a 180° (abajo) en sentido horario (CW) representa velocidad 0-100% CW
        // 180° (abajo) a 360° (arriba) en sentido horario (CW) representa velocidad 100-0% CCW

        if (normalizedAngle >= 0 && normalizedAngle <= 180) { // Lado derecho (CW)
            speedPercentage = map(normalizedAngle, 0, 180, 0, 100);
            directionLabel = 'Derecha (CW)';
        } else { // Lado izquierdo (CCW)
            speedPercentage = map(normalizedAngle, 360, 180, 0, 100); // 360 -> 0%, 180 -> 100%
            directionLabel = 'Izquierda (CCW)';
        }

        // Ajustar visualmente el knob
        speedDirectionKnob.style.background = `conic-gradient(#00ff88 ${normalizedAngle}deg, #4d4d4d ${normalizedAngle}deg)`;
        speedDirectionKnob.style.transform = `rotate(${normalizedAngle - 90}deg)`; // Para que el 0° sea arriba

        knobSpeedValue.textContent = `${Math.round(speedPercentage)}%`;
        knobDirectionValue.textContent = `${Math.round(normalizedAngle)}° (${directionLabel})`;

        currentSpeedPercentage = Math.round(speedPercentage);
        currentDirectionAngle = Math.round(normalizedAngle); // Guarda el ángulo raw para decidir dirección
    }

    // Inicializar knob a 0% velocidad y dirección nula (apuntando hacia arriba)
    let initialKnobAngle = 0; // 0 grados apunta hacia arriba en nuestro mapeo
    updateKnob(initialKnobAngle);

    // Lógica para arrastrar el knob
    speedDirectionKnob.addEventListener('mousedown', (e) => {
        isKnobDragging = true;
        speedDirectionKnob.classList.add('active');
        e.preventDefault(); // Evitar selección de texto
    });

    document.addEventListener('mousemove', (e) => {
        if (!isKnobDragging) return;

        const knobRect = speedDirectionKnob.getBoundingClientRect();
        const centerX = knobRect.left + knobRect.width / 2;
        const centerY = knobRect.top + knobRect.height / 2;

        const deltaX = e.clientX - centerX;
        const deltaY = e.clientY - centerY;

        let angle = Math.atan2(deltaY, deltaX) * (180 / Math.PI);
        // Ajustar el ángulo para que 0 sea hacia arriba y gire en sentido horario 0-360
        angle = (angle + 90 + 360) % 360; // 0 (up) to 360 (up CW)

        updateKnob(angle);
    });

    document.addEventListener('mouseup', () => {
        if (isKnobDragging) {
            isKnobDragging = false;
            speedDirectionKnob.classList.remove('active');
        }
    });

    // --- Animación del Motor y Flecha de Dirección ---
    function startMotorAnimation(direction, speed) {
        // Detener cualquier animación previa
        stopMotorAnimation();

        // Determinar la clase de animación y duración
        let animationClass = '';
        if (direction === 'right') {
            animationClass = 'spin-cw';
        } else if (direction === 'left') {
            animationClass = 'spin-ccw';
        }

        // La velocidad en la UI es 0-100%, mapeamos a una duración (e.g., 2s lento a 0.1s rápido)
        // Inverso: mayor speedPercentage = menor duration
        const animationDuration = speed > 0 ? `${map(speed, 0, 100, 2, 0.1)}s` : '0s';

        // Aplicar la animación a la imagen del motor
        if (animationClass && speed > 0) {
            motorAnimation.style.animation = `${animationClass} ${animationDuration} linear infinite`;
        } else {
            motorAnimation.style.animation = 'none'; // Detener si velocidad es 0
        }

        // Actualizar la flecha de dirección
        if (speed > 0) { // Solo si el motor realmente está girando
             if (direction === 'right') {
                directionArrow.style.transform = `rotate(90deg)`;
            } else if (direction === 'left') {
                directionArrow.style.transform = `rotate(-90deg)`;
            }
        } else {
            directionArrow.style.transform = `rotate(0deg)`; // Hacia arriba cuando está parado
        }
    }

    function stopMotorAnimation() {
        motorAnimation.style.animation = 'none';
        directionArrow.style.transform = `rotate(0deg)`; // Resetea la flecha a arriba
    }

    function updateMotorAnimation() {
        // La animación solo debe correr si el motor está "activo" Y está en modo continuo
        // O si está haciendo un movimiento específico (no continuo) y aún no ha terminado.
        // Pero para simplificar, la animación la manejamos según el estado 'motorEnabled' y 'currentMotorSpeed'.
        // Si el torque está habilitado y el motor no gira, no hay animación pero sí torque.
        if (isMotorOn && currentMotorSpeed > 0) {
            startMotorAnimation(currentMotorDirection, currentMotorSpeed);
        } else {
            stopMotorAnimation();
        }
    }

    // --- Event Listeners ---
    refreshStatusBtn.addEventListener('click', updateStatus);

    motorOnBtn.addEventListener('click', () => {
        sendCommand('enable');
    });

    motorOffBtn.addEventListener('click', () => {
        sendCommand('disable');
    });

    sendKnobDataBtn.addEventListener('click', () => {
        // Enviar velocidad
        sendCommand('speed', currentSpeedPercentage);

        // Enviar dirección solo si la velocidad es > 0, si no, se detiene
        if (currentSpeedPercentage > 0) {
            let directionCmd = (currentDirectionAngle >= 0 && currentDirectionAngle <= 180) ? 'right' : 'left';
            sendCommand('direction', directionCmd);
        } else {
            sendCommand('direction', 'stop'); // Detener giro continuo si la velocidad es 0
        }
    });

    halfStepModeBtn.addEventListener('click', () => {
        sendCommand('step_mode', 'half');
    });

    fullStepModeBtn.addEventListener('click', () => {
        sendCommand('step_mode', 'full');
    });

    rotateByAngleBtn.addEventListener('click', async () => {
        const angle = parseInt(angleInput.value);
        if (isNaN(angle) || angle < 0 || angle > 360) {
            alert('Por favor, ingresa un ángulo válido entre 0 y 360 grados.');
            return;
        }
        sendCommand('rotate_angle', angle);
    });

    fullRevolutionBtn.addEventListener('click', () => {
        // 360 grados = actualStepsPerRevolution (siempre CW para este botón)
        // Se calcula el número de pasos en el Arduino, enviamos 360 grados
        sendCommand('rotate_angle', 360);
    });

    halfRevolutionBtn.addEventListener('click', () => {
        // 180 grados = actualStepsPerRevolution / 2 (siempre CW para este botón)
        // Se calcula el número de pasos en el Arduino, enviamos 180 grados
        sendCommand('rotate_angle', 180);
    });

    oscillateBtn.addEventListener('click', () => {
        sendCommand('oscillate');
    });

    toggleTorqueBtn.addEventListener('click', () => {
        if (torqueState) {
            sendCommand('disable_torque');
        } else {
            sendCommand('enable_torque');
        }
    });

    returnToZeroBtn.addEventListener('click', () => {
        sendCommand('return_to_zero');
    });

    // --- Inicialización ---
    updateStatus(); // Obtener el estado inicial al cargar la página
    setInterval(updateStatus, 1000); // Actualizar estado cada 1 segundo (más frecuente para el motor)

    // Cargar imagen de motor estático por defecto
    motorAnimation.src = 'motor_static.png'; // Asegúrate de tener esta imagen
    directionArrow.src = 'arrow_up.png'; // Asegúrate de tener esta imagen
});