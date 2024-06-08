#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>
#include <color_spaces.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

constexpr int bailoutRadius = 100;
constexpr int max_iterations = 1000;

constexpr int previewTextureSizeFactor = 10;
constexpr float baseForZoomScrollFunction = 0.5;
constexpr int howManyPixelsToComputePerAsyncMandelbrotResume = (1000 * 1000) / 100;

struct complex
{
    float r;
    float i;
};

void printComplex(complex c)
{
    std::cout << c.r << " + " << c.i << "i\n";
}

namespace state
{

    GLFWwindow *window;
    GLuint shaderProgram;

    int currentWidth = 1000;
    int currentHeight = 1000;
    std::vector<unsigned char> textureImage;

    complex centralPoint{-0.5, 0};
    float zoom = 3;
}

enum
{
    is_in_mandelbrot_set = -10,
};
inline float smooth_iteration_count(complex &c)
{
    complex z{0, 0};

    for (int i = 0; i < max_iterations; i++)
    {
        // z = z^2 + c
        float tempZI = 2 * z.r * z.i + c.i;
        z.r = z.r * z.r - z.i * z.i + c.r;
        z.i = tempZI;

        float absoluteCsquared = z.r * z.r + z.i * z.i;
        if (absoluteCsquared > bailoutRadius * bailoutRadius)
        {
            float smoother = 2.0 - log2(log(absoluteCsquared));
            return i + smoother;
        }
    }

    return is_in_mandelbrot_set;
}
inline float smooth_iteration_count(complex &&c)
{
    return smooth_iteration_count(c);
}

void colorThisPartBasedOnIterationCount(unsigned char RGBarray[], float iterations_number_took)
{
    if (iterations_number_took == is_in_mandelbrot_set)
    {
        RGBarray[0] = 0; // R
        RGBarray[1] = 0; // G
        RGBarray[2] = 0; // B
    }
    else
    {
        HSV hsvColor{(float)fmod(iterations_number_took * 10 + 240, 360), 1, 1};
        RGB rgbColor = HSVtoRGB(hsvColor);
        RGBarray[0] = rgbColor.r; // R
        RGBarray[1] = rgbColor.g; // G
        RGBarray[2] = rgbColor.b; // B
    }
}

void newTextureSize(std::vector<unsigned char> &newTextureData, int width, int height, GLuint shaderProgram)
{
    // Update texture
    // binding maybe unnecessary glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, newTextureData.data());

    // Bind the texture to the shader uniform
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
}

void updateTextureWithSameSize(std::vector<unsigned char> &newTextureData, int width, int height, GLuint shaderProgram)
{
    // Update texture
    // binding maybe unnecessary glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, newTextureData.data());

    // Bind the texture to the shader uniform
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
}

void getNormalizedCursorPositionInWindow(GLFWwindow *window, double &normalizedX, double &normalizedY)
{
    // Get the cursor position in pixels
    double cursorX, cursorY;
    glfwGetCursorPos(window, &cursorX, &cursorY);

    // Normalize the cursor position
    normalizedX = cursorX / double(state::currentWidth);
    normalizedY = (double(state::currentHeight) - cursorY) / double(state::currentHeight);
}

complex getComplexNumberCursorPointsToInWindow(GLFWwindow *window)
{
    using namespace state;
    double cursorX, cursorY;
    getNormalizedCursorPositionInWindow(window, cursorX, cursorY);

    float highestOfThem = (currentHeight > currentWidth) ? currentHeight : currentWidth;

    complex n;
    n.r = centralPoint.r + (cursorX - 0.5) * (currentWidth / highestOfThem) * zoom;
    n.i = centralPoint.i + (cursorY - 0.5) * (currentHeight / highestOfThem) * zoom;

    return n;
}

void getNormalizedPositionCursorIsInZoomSpace(double &normalizedX, double &normalizedY, GLFWwindow *window)
{
    using namespace state;
    double cursorX, cursorY;
    getNormalizedCursorPositionInWindow(window, cursorX, cursorY);

    float highestOfThem = (currentHeight > currentWidth) ? currentHeight : currentWidth;

    normalizedX = (cursorX - 0.5) * (currentWidth / highestOfThem) + 0.5;
    normalizedY = (cursorY - 0.5) * (currentHeight / highestOfThem) + 0.5;
}

complex numberCentralShouldBeToMakePointBeInNormalizedZoomSpace(complex point, float zoom, float Nwidth, float Nheight)
{
    return complex{-zoom * Nwidth + point.r + zoom / 2, -zoom * Nheight + point.i + zoom / 2};
}

complex numberCentralShouldBeToMakePointBeInNormalizedWindow(complex point, float zoom, float Nwidth, float Nheight)
{
    using namespace state;
    float highestOfThem = (currentHeight > currentWidth) ? currentHeight : currentWidth;
    complex n;
    n.r = -(float(Nwidth) - 0.5f) * (float(currentWidth) / highestOfThem) * zoom + point.r;
    n.i = -(float(Nheight) - 0.5f) * (float(currentHeight) / highestOfThem) * zoom + point.i;
    return n;
}

namespace mandelbrotCalculator
{
    void computeMandelbrot(std::vector<unsigned char> &textureData, float zoom, complex centralPoint, int width, int height)
    {
        textureData.resize(width * height * 3); // RGB format: 3 bytes per pixel

        float highestOfThem = (height > width) ? height : width;

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int index = (y * width + x) * 3;

                complex c;
                c.r = (float(x) / float(width)) * zoom * (float(width) / highestOfThem) - zoom * (float(width) / highestOfThem) / 2 + centralPoint.r;
                c.i = (float(y) / float(height)) * zoom * (float(height) / highestOfThem) - zoom * (float(height) / highestOfThem) / 2 + centralPoint.i;

                float iterations_number_took = smooth_iteration_count(c);

                colorThisPartBasedOnIterationCount(textureData.data() + index, iterations_number_took);
            }
        }
    }

}

namespace mandelbrotCalculator::asyncMandelbrot
{
    namespace asyncMandelbrotState
    {
        float highestOfThem;
        std::vector<unsigned char> textureData;
        float zoom;
        complex centralPoint;
        int width;
        int height;
        int x;
        int y;
        bool isComputing = false;
        bool isTextureReadyForUsage = false;
    }

    void stop()
    {
        using namespace asyncMandelbrotState;
        isComputing = false;
    }

    inline bool isItComputing()
    {
        using namespace asyncMandelbrotState;
        return isComputing;
    }

    inline bool isTextureReady()
    {
        using namespace asyncMandelbrotState;
        return isTextureReadyForUsage;
    }

    inline void imUingTheTextureRightNow()
    {
        using namespace asyncMandelbrotState;
        isTextureReadyForUsage = false;
    }

    inline std::vector<unsigned char> &getTexture()
    {
        using namespace asyncMandelbrotState;
        return textureData;
    }

    void compute(float zoom_, complex centralPoint_, int width_, int height_)
    {
        using namespace asyncMandelbrotState;
        isTextureReadyForUsage = false;
        if (isComputing)
        {
            std::cout << "tried to start a computation when another was still being computed\n";
            exit(-1);
        }
        textureData.resize(width_ * height_ * 3);
        highestOfThem = (width_ > height_) ? width_ : height_;
        zoom = zoom_;
        centralPoint = centralPoint_;
        width = width_;
        height = height_;
        x = 0;
        y = 0;
        isComputing = true;
    }

    void resume(unsigned int howManyTimes)
    {
        using namespace asyncMandelbrotState;
        if (!isComputing)
        {
            std::cout << "tried to resume when its not computing\n";
            exit(-1);
        }

        int starting_x = x;

        for (; y < height; ++y)
        {

            for (x = starting_x; x < width; ++x)
            {

                if (howManyTimes == 0)
                {
                    return;
                }
                --howManyTimes;

                int index = (y * width + x) * 3;

                complex c;
                c.r = (float(x) / float(width)) * zoom * (float(width) / highestOfThem) - zoom * (float(width) / highestOfThem) / 2 + centralPoint.r;
                c.i = (float(y) / float(height)) * zoom * (float(height) / highestOfThem) - zoom * (float(height) / highestOfThem) / 2 + centralPoint.i;

                float iterations_number_took = smooth_iteration_count(c);

                colorThisPartBasedOnIterationCount(textureData.data() + index, iterations_number_took);
            }

            starting_x = 0;
        }

        stop();
        isTextureReadyForUsage = true;
    }

}

namespace mandelbrotCalculator::parallelMandelbrot
{

    namespace parallelMandelbrotState
    {
        std::vector<std::thread> threadPool;
        std::vector<bool> readyThreadArray;
        bool hasTextureBeenUsed = false;
        bool haveIalreadyJoined = true;
        std::atomic<bool> shouldStop = false;
        int num_threads;

        void setReadyThread(bool a)
        {
            for (int i = 0; i < num_threads; ++i)
            {
                readyThreadArray[i] = a;
            }
        }
    }

    bool isComputing()
    {
        using namespace parallelMandelbrotState;
        bool haveAllThreadsCompleted = true;
        for (int i = 0; i < num_threads; ++i)
        {
            haveAllThreadsCompleted = haveAllThreadsCompleted && readyThreadArray[i];
        }
        // if at least one false, everything is false. AND everything
        return !haveAllThreadsCompleted;
    }

    bool isTextureReady()
    {
        using namespace parallelMandelbrotState;
        if (hasTextureBeenUsed)
        {
            return false;
        }

        return !isComputing();
    }

    void imUsingTheTexture()
    {
        using namespace parallelMandelbrotState;
        if (hasTextureBeenUsed == true)
        {
            std::cout << "tried to use texture two times\n";
            exit(-1);
        }
        hasTextureBeenUsed = true;
    }

    void initialize()
    {
        using namespace parallelMandelbrotState;

        num_threads = std::thread::hardware_concurrency() - 1;
        if(num_threads == 0){
            num_threads = 1;
        }

        threadPool.resize(num_threads);
        readyThreadArray.resize(num_threads);

        setReadyThread(true);
    }

    void computePiece(std::vector<unsigned char> &textureData, float zoom, complex centralPoint, int width, int height, int begin, unsigned int howManyPixels, int myThreadID)
    {
        using namespace parallelMandelbrotState;

        float highestOfThem = (height > width) ? height : width;

        int start_y = begin / width;
        int start_x = begin % width;

        // unconditional loop because howManyPixels does the job of going the right amount of pixels
        for (int y = start_y;; ++y)
        {
            if (shouldStop.load())
            {
                // readyThreadArray[myThreadID] = true;
                return;
            }

            for (int x = start_x; x < width; ++x)
            {

                if (howManyPixels == 0)
                {
                    readyThreadArray[myThreadID] = true;
                    return;
                }
                --howManyPixels;

                int index = (y * width + x) * 3;

                complex c;
                c.r = (float(x) / float(width)) * zoom * (float(width) / highestOfThem) - zoom * (float(width) / highestOfThem) / 2 + centralPoint.r;
                c.i = (float(y) / float(height)) * zoom * (float(height) / highestOfThem) - zoom * (float(height) / highestOfThem) / 2 + centralPoint.i;

                float iterations_number_took = smooth_iteration_count(c);

                colorThisPartBasedOnIterationCount(textureData.data() + index, iterations_number_took);
            }
            start_x = 0;
        }
    }

    void join()
    {
        using namespace parallelMandelbrotState;
        haveIalreadyJoined = true;
        for (int i = 0; i < num_threads; ++i)
        {
            threadPool[i].join();
        }
    }

    void computeParallel(std::vector<unsigned char> &textureData, float zoom, complex centralPoint, int width, int height)
    {
        using namespace parallelMandelbrotState;

        if (isComputing())
        {
            std::cout << "tried to start a computation while another was still running\n";
            exit(-1);
        }

        if(!haveIalreadyJoined){
            join();
        }
        haveIalreadyJoined = false;

        textureData.resize(width * height * 3); // RGB format: 3 bytes per pixel

        setReadyThread(false);
        hasTextureBeenUsed = false;

        int nPixelsPerThread = (width * height) / num_threads;
        int nExtraPixels = (width * height) % num_threads; // special threads will compute one extra pixel. We are diving as even as possible

        int startPixel = 0;
        for (int i = 0; i < num_threads - nExtraPixels; ++i)
        {
            threadPool[i] = std::thread(computePiece, std::ref(textureData), zoom, centralPoint, width, height, startPixel, nPixelsPerThread, i);
            startPixel += nPixelsPerThread;
        }
        for (int i = num_threads - nExtraPixels; i < num_threads; ++i)
        {
            threadPool[i] = std::thread(computePiece, std::ref(textureData), zoom, centralPoint, width, height, startPixel, nPixelsPerThread + 1, i);
            startPixel += nPixelsPerThread + 1;
        }
    }

    void stop()
    {
        using namespace parallelMandelbrotState;
        shouldStop.store(true);
        join();
        shouldStop.store(false);
        setReadyThread(true);
        hasTextureBeenUsed = true;
    }

    void stopIfComputing(){
        if(isComputing()){
            stop();
        }
    }
}

namespace inputHandler
{

    bool isWindowInFocus = true;
    void windowFocusCallback(GLFWwindow *window, int focused)
    {
        isWindowInFocus = focused;
    }

    bool shouldResize = false;
    void framebuffer_size_callback(GLFWwindow *window, int width, int height)
    {
        state::currentWidth = width;
        state::currentHeight = height;
        shouldResize = true;
    }

    bool isInDragMode = false;
    complex dragPoint;
    void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
    {
        if (!isWindowInFocus)
        {
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            if (action == GLFW_PRESS)
            {
                dragPoint = getComplexNumberCursorPointsToInWindow(window);
                isInDragMode = true;
                mandelbrotCalculator::parallelMandelbrot::stopIfComputing();
            }

            if (action == GLFW_RELEASE)
            {
                isInDragMode = false;

                mandelbrotCalculator::parallelMandelbrot::computeParallel(state::textureImage, state::zoom, state::centralPoint, state::currentWidth, state::currentHeight);
            }
        }
    }

    bool shouldZoomScroll = false;
    double yOffsetLastScroll;
    void scrollCallback(GLFWwindow *window, double xOffset, double yOffset)
    {
        shouldZoomScroll = true;
        yOffsetLastScroll = yOffset;
    }

    void runEvents()
    {
        if (shouldResize)
        {
            mandelbrotCalculator::parallelMandelbrot::stopIfComputing();

            glViewport(0, 0, state::currentWidth, state::currentHeight);

            // Regenerate texture data
            std::vector<unsigned char> newTextureData;

            int previewWidth = state::currentWidth / previewTextureSizeFactor;
            int previewHeight = state::currentHeight / previewTextureSizeFactor;

            mandelbrotCalculator::parallelMandelbrot::computeParallel(newTextureData, state::zoom, state::centralPoint, previewWidth, previewHeight);
            mandelbrotCalculator::parallelMandelbrot::join();
            newTextureSize(newTextureData, previewWidth, previewHeight, state::shaderProgram);
            mandelbrotCalculator::parallelMandelbrot::imUsingTheTexture();

            mandelbrotCalculator::parallelMandelbrot::computeParallel(state::textureImage, state::zoom, state::centralPoint, state::currentWidth, state::currentHeight);

            shouldResize = false;
        }

        if (isInDragMode)
        {
            double x, y;
            getNormalizedCursorPositionInWindow(state::window, x, y);
            state::centralPoint = numberCentralShouldBeToMakePointBeInNormalizedWindow(dragPoint, state::zoom, x, y);
            std::vector<unsigned char> texture;

            int previewWidth = state::currentWidth / previewTextureSizeFactor;
            int previewHeight = state::currentHeight / previewTextureSizeFactor;

            mandelbrotCalculator::parallelMandelbrot::computeParallel(texture, state::zoom, state::centralPoint, previewWidth, previewHeight);
            mandelbrotCalculator::parallelMandelbrot::join();
            newTextureSize(texture, previewWidth, previewHeight, state::shaderProgram);
            mandelbrotCalculator::parallelMandelbrot::imUsingTheTexture();
        }

        if (shouldZoomScroll)
        {
            mandelbrotCalculator::parallelMandelbrot::stopIfComputing();
            complex zoomPoint = getComplexNumberCursorPointsToInWindow(state::window);

            double x, y;
            getNormalizedCursorPositionInWindow(state::window, x, y);

            float newZoom = state::zoom * std::pow(baseForZoomScrollFunction, float(yOffsetLastScroll));
            state::centralPoint = numberCentralShouldBeToMakePointBeInNormalizedWindow(zoomPoint, newZoom, x, y);
            state::zoom = newZoom;
            std::vector<unsigned char> texture;

            int previewWidth = state::currentWidth / previewTextureSizeFactor;
            int previewHeight = state::currentHeight / previewTextureSizeFactor;

            mandelbrotCalculator::parallelMandelbrot::computeParallel(texture, state::zoom, state::centralPoint, previewWidth, previewHeight);
            mandelbrotCalculator::parallelMandelbrot::join();
            newTextureSize(texture, previewWidth, previewHeight, state::shaderProgram);
            mandelbrotCalculator::parallelMandelbrot::imUsingTheTexture();

            mandelbrotCalculator::parallelMandelbrot::computeParallel(state::textureImage, state::zoom, state::centralPoint, state::currentWidth, state::currentHeight);

            shouldZoomScroll = false;
        }
    }

}

int main()
{

    GLuint VBO, VAO;
    GLuint texture;

    mandelbrotCalculator::parallelMandelbrot::initialize();

    { // things which i dont know exactly how they work
        // Initialize GLFW
        if (!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return -1;
        }

        // Create a windowed mode window and its OpenGL context
        state::window = glfwCreateWindow(state::currentWidth, state::currentHeight, "Texture Example", NULL, NULL);
        if (!state::window)
        {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return -1;
        }

        // Make the window's context current
        glfwMakeContextCurrent(state::window);

        // Initialize GLAD
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return -1;
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // because its RBG

        // Create and bind a texture
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        {
            std::vector<unsigned char> textureData(1000 * 1000 * 3);
            mandelbrotCalculator::parallelMandelbrot::computeParallel(textureData, state::zoom, state::centralPoint, state::currentWidth, state::currentHeight);
            mandelbrotCalculator::parallelMandelbrot::join();
            mandelbrotCalculator::parallelMandelbrot::imUsingTheTexture();
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, state::currentWidth, state::currentHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // nearest for a more pixelated look

        // Create a vertex buffer object (VBO) and vertex array object (VAO) for the quad
        glGenBuffers(1, &VBO);
        glGenVertexArrays(1, &VAO);

        // Bind the VAO and VBO, and buffer the vertex and texture coordinate data
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        float vertices[] = {
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 0.0f, 1.0f, 1.0f};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        // Texture coordinate attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // Create and compile shaders
        const char *vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main()
        {
           gl_Position = vec4(aPos, 1.0);
           TexCoord = aTexCoord;
        })";
        const char *fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D texture1;
        void main()
        {
           FragColor = texture(texture1, TexCoord);
        })";

        // Compile shaders
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        // Link shaders
        state::shaderProgram = glCreateProgram();
        glAttachShader(state::shaderProgram, vertexShader);
        glAttachShader(state::shaderProgram, fragmentShader);
        glLinkProgram(state::shaderProgram);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // Use the shader program
        glUseProgram(state::shaderProgram);
        glUniform1i(glGetUniformLocation(state::shaderProgram, "texture1"), 0);
    }

    glfwSetFramebufferSizeCallback(state::window, inputHandler::framebuffer_size_callback);
    glfwSetMouseButtonCallback(state::window, inputHandler::mouseButtonCallback);
    glfwSetWindowFocusCallback(state::window, inputHandler::windowFocusCallback);
    glfwSetScrollCallback(state::window, inputHandler::scrollCallback);

    // main loop
    while (!glfwWindowShouldClose(state::window))
    {

        inputHandler::runEvents();

        if (mandelbrotCalculator::parallelMandelbrot::isTextureReady())
        {
            newTextureSize(state::textureImage, state::currentWidth, state::currentHeight, state::shaderProgram);
            mandelbrotCalculator::parallelMandelbrot::imUsingTheTexture();
        }

        // Render
        glClear(GL_COLOR_BUFFER_BIT);

        // Bind texture
        // binding maybe unnecessary glActiveTexture(GL_TEXTURE0);
        // binding maybe unnecessary glBindTexture(GL_TEXTURE_2D, texture);

        // Render quad
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Swap buffers
        glfwSwapBuffers(state::window);

        // Poll for and process events
        glfwPollEvents();
    }

    { // clean up
        if(!mandelbrotCalculator::parallelMandelbrot::parallelMandelbrotState::haveIalreadyJoined){
            mandelbrotCalculator::parallelMandelbrot::join();
        }
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteTextures(1, &texture);
        glDeleteProgram(state::shaderProgram);
        glfwTerminate();
    }

    return 0;
}