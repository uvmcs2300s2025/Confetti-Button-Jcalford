#include "engine.h"

enum state {start, play, over};
state screen;

// Colors
color originalFill, hoverFill, pressFill;

Engine::Engine() : keys() {
    this->initWindow();
    this->initShaders();
    this->initShapes();

    originalFill = {1, 0, 0, 1};
    hoverFill.vec = originalFill.vec + vec4{0.5, 0.5, 0.5, 0};
    pressFill.vec = originalFill.vec - vec4{0.5, 0.5, 0.5, 0};
}

Engine::~Engine() {}

unsigned int Engine::initWindow(bool debug) {
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, false);

    window = glfwCreateWindow(width, height, "engine", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    // OpenGL configuration
    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSwapInterval(1);

    return 0;
}

void Engine::initShaders() {
    // load shader manager
    shaderManager = make_unique<ShaderManager>();

    // Load shader into shader manager and retrieve it
    shapeShader = this->shaderManager->loadShader("../res/shaders/shape.vert", "../res/shaders/shape.frag",  nullptr, "shape");

    // Configure text shader and renderer
    textShader = shaderManager->loadShader("../res/shaders/text.vert", "../res/shaders/text.frag", nullptr, "text");
    fontRenderer = make_unique<FontRenderer>(shaderManager->getShader("text"), "../res/fonts/MxPlus_IBM_BIOS.ttf", 24);

    // Set uniforms
    textShader.setVector2f("vertex", vec4(100, 100, .5, .5));
    shapeShader.use();
    shapeShader.setMatrix4("projection", this->PROJECTION);
}

void Engine::initShapes() {
    // red spawn button centered in the top left corner
    spawnButton = make_unique<Rect>(shapeShader, vec2{width/2, height/2}, vec2{100, 50}, color{1, 0, 0, 1});
}

void Engine::processInput() {
    glfwPollEvents();

    // Set keys to true if pressed, false if released
    for (int key = 0; key < 1024; ++key) {
        if (glfwGetKey(window, key) == GLFW_PRESS)
            keys[key] = true;
        else if (glfwGetKey(window, key) == GLFW_RELEASE)
            keys[key] = false;
    }

    // Close window if escape key is pressed
    if (keys[GLFW_KEY_ESCAPE])
        glfwSetWindowShouldClose(window, true);

    // Mouse position saved to check for collisions
    glfwGetCursorPos(window, &MouseX, &MouseY);

    // If we're in the start screen and the user presses s, change screen to play
    if (screen == start && keys[GLFW_KEY_S])
        screen = play;

    // If we're in the play screen and an arrow key is pressed, move the spawnButton
    if (screen == play) {
        float speed = 200.0f * deltaTime; // Adjust speed as needed
        vec2 newPos = spawnButton->getPos();
        if (keys[GLFW_KEY_UP])
            newPos.y += speed;
        if (keys[GLFW_KEY_DOWN])
            newPos.y -= speed;
        if (keys[GLFW_KEY_LEFT])
            newPos.x -= speed;
        if (keys[GLFW_KEY_RIGHT])
            newPos.x += speed;

        // Make sure the spawnButton cannot go off the screen
        newPos.x = glm::clamp(newPos.x, spawnButton->getSize().x / 2.0f, width - spawnButton->getSize().x / 2.0f);
        newPos.y = glm::clamp(newPos.y, spawnButton->getSize().y / 2.0f, height - spawnButton->getSize().y / 2.0f);
        spawnButton->setPos(newPos);
    }

    // Mouse position is inverted because the origin of the window is in the top left corner
    MouseY = height - MouseY; // Invert y-axis of mouse position
    bool buttonOverlapsMouse = spawnButton->isOverlapping(vec2(MouseX, MouseY));
    bool mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // When in play screen, handle button color changes and confetti spawning
    if (screen == play) {
        // Change spawnButton's color based on hover or click
        if (buttonOverlapsMouse && mousePressed) {
            spawnButton->setColor(pressFill);
        } else if (buttonOverlapsMouse) {
            spawnButton->setColor(hoverFill);
        } else {
            spawnButton->setColor(originalFill);
        }

        // If the button was released, spawn confetti
        if (buttonOverlapsMouse && mousePressedLastFrame && !mousePressed) {
            spawnConfetti();
        }
    }

    // Save mousePressed for next frame
    mousePressedLastFrame = mousePressed;
}

void Engine::update() {
    // Calculate delta time
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // End the game when the user spawns 100 confetti
    if (confetti.size() >= 100) {
        screen = over;
    }
}

void Engine::render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Set background color
    glClear(GL_COLOR_BUFFER_BIT);

    // Set shader to draw shapes
    shapeShader.use();

    // Render differently depending on screen
    switch (screen) {
        case start: {
            string message = "Press s to start";
            // (12 * message.length()) is the offset to center text.
            // 12 pixels is the width of each character scaled by 1.
            this->fontRenderer->renderText(message, width/2 - (12 * message.length()), height/2, projection, 1, vec3{1, 1, 1});
            break;
        }
        case play: {
            // Draw all confetti pieces
            for (const auto& piece : confetti) {
                piece->setUniforms();
                piece->draw();
            }
            // Draw spawn button last to appear on top
            spawnButton->setUniforms();
            spawnButton->draw();
            // Render font on top of spawn button
            fontRenderer->renderText("Spawn", spawnButton->getPos().x - 30, spawnButton->getPos().y - 5, projection, 0.5, vec3{1, 1, 1});
            break;
        }
        case over: {
            string message = "You win!";
            // Display the message on the screen
            this->fontRenderer->renderText(message, width/2 - (12 * message.length()), height/2, projection, 1, vec3{1, 1, 1});
            break;
        }
    }

    glfwSwapBuffers(window);
}

void Engine::spawnConfetti() {
    vec2 pos = {rand() % (int)width, rand() % (int)height};
    // Make each piece of confetti a different size, getting bigger with each spawn
    float sizeFactor = 1.0f + (confetti.size() * (100.0f - 1.0f) / 99.0f); // Linearly interpolate from 1 to 100
    vec2 size = {sizeFactor, sizeFactor};
    color color = {float(rand() % 10 / 10.0), float(rand() % 10 / 10.0), float(rand() % 10 / 10.0), 1.0f};
    confetti.push_back(make_unique<Rect>(shapeShader, pos, size, color));
}

bool Engine::shouldClose() {
    return glfwWindowShouldClose(window);
}

GLenum Engine::glCheckError_(const char *file, int line) {
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        string error;
        switch (errorCode) {
            case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
            case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        cout << error << " | " << file << " (" << line << ")" << endl;
    }
    return errorCode;
}