#include "glad.h"
#include "mainwindow.h"
#include "qmdlrenderer.h"

struct GPUAxisData
{
    QVector3D               position;
    std::array<uint8_t, 4>  color;
};

QMDLRenderer::QMDLRenderer(QWidget *parent) :
    QOpenGLWidget(parent)
{
    setMouseTracking(true);

    _camera.setBehavior(Camera::CameraBehavior::CAMERA_BEHAVIOR_ORBIT);
    _camera.lookAt({ 25.f, 0, 0 }, {}, { 0, 1, 0 });
	_camera.zoom(Camera::DEFAULT_ORBIT_OFFSET_DISTANCE, _camera.getOrbitMinZoom(), _camera.getOrbitMaxZoom());
    
    _horizontalSplit = MainWindow::instance().settings.value("HorizontalSplit", 0.5f).toFloat();
    _verticalSplit = MainWindow::instance().settings.value("VerticalSplit", 0.5f).toFloat();
}

void QMDLRenderer::generateGrid(float grid_size, size_t count)
{
    size_t point_count = ((count * 4) + 4) * 2;
    std::vector<QVector3D> points;
    points.reserve(point_count);

    // corners
    float extreme = grid_size * count;
        
    points.emplace_back(-extreme, -extreme, 0);
    points.emplace_back(extreme, -extreme, 0);
        
    points.emplace_back(-extreme, extreme, 0);
    points.emplace_back(extreme, extreme, 0);
        
    points.emplace_back(-extreme, -extreme, 0);
    points.emplace_back(-extreme, extreme, 0);
        
    points.emplace_back(extreme, extreme, 0);
    points.emplace_back(extreme, -extreme, 0);

    // lines
    for (size_t i = 0; i < count; i++)
    {
        float v = grid_size * i;

        // x
        points.emplace_back(v, -extreme, 0);
        points.emplace_back(v, extreme, 0);
        points.emplace_back(-v, -extreme, 0);
        points.emplace_back(-v, extreme, 0);

        // y
        points.emplace_back(-extreme, v,  0);
        points.emplace_back(extreme, v, 0);
        points.emplace_back(-extreme, -v, 0);
        points.emplace_back(extreme, -v, 0);
    }

    glBufferData(GL_ARRAY_BUFFER, point_count * sizeof(QVector3D), points.data(), GL_STATIC_DRAW);
    _gridSize = point_count;
}

void QMDLRenderer::generateAxis()
{
    constexpr GPUAxisData axisData[] = {
        { { 0.0f, 0.0f, 0.0f }, { 255, 0, 0, 127 } },
        { { 32.0f, 0.0f, 0.0f }, { 255, 0, 0, 127 } },

        { { 0.0f, 0.0f, 0.0f }, { 0, 255, 0, 127 } },
        { { 0.0f, 32.0f, 0.0f }, { 0, 255, 0, 127 } },

        { { 0.0f, 0.0f, 0.0f }, { 0, 0, 255, 127 } },
        { { 0.0f, 0.0f, 32.0f }, { 0, 0, 255, 127 } },
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(axisData), axisData, GL_STATIC_DRAW);
}

template<size_t w, size_t h, size_t N = w * h * 4>
static inline void createBuiltInTexture(GLuint &out, const std::initializer_list<byte> &pixels)
{
    if (pixels.size() != N)
        throw std::runtime_error("bad pixels");

    glGenTextures(1, &out);
    glBindTexture(GL_TEXTURE_2D, out);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.begin());
}

void QMDLRenderer::initializeGL()
{
    gladLoadGL();

    glFrontFace(GL_CW);

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
        
    createBuiltInTexture<1, 1>(_whiteTexture, { 0xFF, 0xFF, 0xFF, 0xFF });
    createBuiltInTexture<1, 1>(_blackTexture, { 0x00, 0x00, 0x00, 0xFF });

    glGenTextures(1, &_modelTexture);

    const char *vertexShaderSource = R"SHADER(
#version 330 core

uniform mat4 u_projection;
uniform mat4 u_modelview;
 
in vec3 i_position;
in vec2 i_texcoord;
in vec4 i_color;

out vec4 v_color;
out vec2 v_texcoord;
 
void main()
{
v_color = i_color;
v_texcoord = i_texcoord;
gl_Position = u_projection * u_modelview * vec4(i_position, 1.0);
}
)SHADER";
 
const char *fragmentShaderSource = R"SHADER(
#version 330 core 
 
precision mediump float;

uniform sampler2D u_texture;

in vec4 v_color;
in vec2 v_texcoord;
 
out vec4 o_color;
 
void main()
{
o_color = texture2D(u_texture, v_texcoord) * v_color;
}
)SHADER";

    GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    _program = createProgram(vertexShader, fragmentShader);
        
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
        
    _projectionUniformLocation = glGetUniformLocation(_program, "u_projection");
    _modelviewUniformLocation = glGetUniformLocation(_program, "u_modelview");

    glUniform1i(glGetUniformLocation(_program, "u_texture"), 0);
        
    _positionAttributeLocation = glGetAttribLocation(_program, "i_position");
    _texcoordAttributeLocation = glGetAttribLocation(_program, "i_texcoord");
    _colorAttributeLocation = glGetAttribLocation(_program, "i_color");

    glGenBuffers(1, &_gridBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _gridBuffer);
    generateGrid(8.0f, 8);

    glGenVertexArrays(1, &_gridVao);
    glBindVertexArray(_gridVao);
    glEnableVertexAttribArray(_positionAttributeLocation);
    glDisableVertexAttribArray(_texcoordAttributeLocation);
    glDisableVertexAttribArray(_colorAttributeLocation);
    glVertexAttribPointer(_positionAttributeLocation, 3, GL_FLOAT, false, sizeof(QVector3D), nullptr);
        
    glGenBuffers(1, &_axisBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _axisBuffer);
    generateAxis();

    glGenVertexArrays(1, &_axisVao);
    glBindVertexArray(_axisVao);
    glEnableVertexAttribArray(_positionAttributeLocation);
    glDisableVertexAttribArray(_texcoordAttributeLocation);
    glEnableVertexAttribArray(_colorAttributeLocation);
    glVertexAttribPointer(_positionAttributeLocation, 3, GL_FLOAT, false, sizeof(GPUAxisData), reinterpret_cast<const GLvoid *>(offsetof(GPUAxisData, position)));
    glVertexAttribPointer(_colorAttributeLocation, 4, GL_UNSIGNED_BYTE, true, sizeof(GPUAxisData), reinterpret_cast<const GLvoid *>(offsetof(GPUAxisData, color)));

    glGenBuffers(1, &_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, _buffer);

    glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);
    glEnableVertexAttribArray(_positionAttributeLocation);
    glEnableVertexAttribArray(_texcoordAttributeLocation);
    glDisableVertexAttribArray(_colorAttributeLocation);
    glVertexAttribPointer(_positionAttributeLocation, 3, GL_FLOAT, false, sizeof(GPUVertexData), reinterpret_cast<const GLvoid *>(offsetof(GPUVertexData, position)));
    glVertexAttribPointer(_texcoordAttributeLocation, 2, GL_FLOAT, false, sizeof(GPUVertexData), reinterpret_cast<const GLvoid *>(offsetof(GPUVertexData, texcoord)));

    glGenBuffers(1, &_pointBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _pointBuffer);

    glGenVertexArrays(1, &_pointVao);
    glBindVertexArray(_pointVao);
    glEnableVertexAttribArray(_positionAttributeLocation);
    glEnableVertexAttribArray(_colorAttributeLocation);
    glDisableVertexAttribArray(_texcoordAttributeLocation);
    glVertexAttribPointer(_positionAttributeLocation, 3, GL_FLOAT, false, sizeof(GPUPointData), reinterpret_cast<const GLvoid *>(offsetof(GPUPointData, position)));
    glVertexAttribPointer(_colorAttributeLocation, 4, GL_UNSIGNED_BYTE, true, sizeof(GPUPointData), reinterpret_cast<const GLvoid *>(offsetof(GPUPointData, color)));
}

void QMDLRenderer::resizeGL(int w, int h)
{
}

QuadRect QMDLRenderer::getQuadrantRect(QuadrantFocus quadrant)
{
    int w = this->width();
    int h = this->height();
        
    // calculate the top-left quadrant size, which is used as the basis
    int qw = (w * _horizontalSplit) - 1;
    int qh = (h * _verticalSplit) - 1;
        
    int line_width = 2 + (w & 1);
    int line_height = 2 + (h & 1);

    if (quadrant == QuadrantFocus::TopLeft)
        return { 0, 0, qw, qh };
    else if (quadrant == QuadrantFocus::Vertical)
        return { qw, 0, line_width, h };
    else if (quadrant == QuadrantFocus::Horizontal)
        return { 0, qh, w, line_height };
    else if (quadrant == QuadrantFocus::Center)
        return { qw, qh, line_width, line_height };

    // the rest of the quadrants need the other side
    // calculated.
    int oqw = w - (qw + line_width);
    int oqh = h - (qh + line_height);

    if (quadrant == QuadrantFocus::TopRight)
        return { qw + line_width, 0, oqw, qh };
    else if (quadrant == QuadrantFocus::BottomRight)
        return { qw + line_width, qh + line_height, oqw, oqh };
    else if (quadrant == QuadrantFocus::BottomLeft)
        return { 0, qh + line_height, qw, oqh };

    throw std::runtime_error("bad quadrant");
}

QuadrantFocus QMDLRenderer::getQuadrantFocus(QPoint xy)
{
    constexpr QuadrantFocus focusOrder[] = {
        QuadrantFocus::Center,
        QuadrantFocus::Horizontal,
        QuadrantFocus::Vertical,
        QuadrantFocus::TopLeft,
        QuadrantFocus::TopRight,
        QuadrantFocus::BottomRight,
        QuadrantFocus::BottomLeft
    };
    QuadRect focusRects[] = {
        getQuadrantRect(QuadrantFocus::Center),
        getQuadrantRect(QuadrantFocus::Horizontal),
        getQuadrantRect(QuadrantFocus::Vertical),
        getQuadrantRect(QuadrantFocus::TopLeft),
        getQuadrantRect(QuadrantFocus::TopRight),
        getQuadrantRect(QuadrantFocus::BottomRight),
        getQuadrantRect(QuadrantFocus::BottomLeft)
    };

    size_t i = 0;

    for (auto &quadrant : focusRects)
    {
        if (xy.x() >= quadrant.x && xy.y() >= quadrant.y &&
            xy.x() < quadrant.x + quadrant.w && xy.y() < quadrant.y + quadrant.h)
            return focusOrder[i];

        i++;
    }

    throw std::runtime_error("bad quadrant");
}

void QMDLRenderer::mousePressEvent(QMouseEvent *e)
{
    _dragging = true;
    _dragButton = e->button();
    _dragPos = e->pos();
}

void QMDLRenderer::mouseReleaseEvent(QMouseEvent *e)
{
    if (!_dragging)
        return;

    _dragging = false;
    
    MainWindow::instance().settings.setValue("HorizontalSplit", _horizontalSplit);
    MainWindow::instance().settings.setValue("VerticalSplit", _verticalSplit);
}

void QMDLRenderer::mouseMoveEvent(QMouseEvent *event)
{
    if (_dragging)
    {
        auto delta = _dragPos - event->pos();

        if (_focusedQuadrant == QuadrantFocus::TopRight)
        {
            if (_dragButton == Qt::MouseButton::RightButton)
                _camera.zoom(-delta.y(), _camera.getOrbitMinZoom(), _camera.getOrbitMaxZoom());
            else if (_dragButton == Qt::MouseButton::LeftButton)
                _camera.rotate(delta.y(), delta.x(), 0);
		    this->update();
        }
        else if (_focusedQuadrant == QuadrantFocus::TopLeft ||
                    _focusedQuadrant == QuadrantFocus::BottomLeft ||
                    _focusedQuadrant == QuadrantFocus::BottomRight)
        {
            if (_dragButton == Qt::MouseButton::RightButton)
            {
                _2dZoom += (delta.y() * 0.01f) * _2dZoom;
		        this->update();
            }
            else if (_dragButton == Qt::MouseButton::LeftButton)
            {
                float xDelta = delta.x() / _2dZoom;
                float yDelta = delta.y() / _2dZoom;

                if (_focusedQuadrant == QuadrantFocus::TopLeft)
                    _2dOffset += QVector3D(-xDelta, yDelta, 0);
                else if (_focusedQuadrant == QuadrantFocus::BottomLeft)
                    _2dOffset += QVector3D(-xDelta, 0, yDelta);
                else
                    _2dOffset += QVector3D(0, -xDelta, yDelta);
		        this->update();
            }
        }
        else
        {
            bool adjust_vert = _focusedQuadrant == QuadrantFocus::Horizontal || _focusedQuadrant == QuadrantFocus::Center;
            bool adjust_horz = _focusedQuadrant == QuadrantFocus::Vertical || _focusedQuadrant == QuadrantFocus::Center;
                
            if (adjust_horz)
                _horizontalSplit = (float) event->pos().x() / width();
            if (adjust_vert)
                _verticalSplit = (float) event->pos().y() / height();

		    this->update();
        }

        _dragPos = event->pos();
        return;
    }

    _focusedQuadrant = getQuadrantFocus(event->pos());
        
    int w = this->width();
    int h = this->height();
        
    int quadrant_w = (w / 2) - 1;
    int quadrant_h = (h / 2) - 1;
        
    int line_width = 2 + (w & 1);
    int line_height = 2 + (h & 1);
        
    bool within_x = event->pos().x() >= quadrant_w && event->pos().x() <= quadrant_w + line_width;
    bool within_y = event->pos().y() >= quadrant_h && event->pos().y() <= quadrant_h + line_height;
        
    if (_focusedQuadrant == QuadrantFocus::Center)
        setCursor(Qt::SizeAllCursor);
    else if (_focusedQuadrant == QuadrantFocus::Horizontal)
        setCursor(Qt::SizeVerCursor);
    else if (_focusedQuadrant == QuadrantFocus::Vertical)
        setCursor(Qt::SizeHorCursor);
    else
        setCursor(Qt::OpenHandCursor);
}

void QMDLRenderer::leaveEvent(QEvent *event)
{
    _focusedQuadrant = QuadrantFocus::None;
}

void QMDLRenderer::clearQuadrant(QuadRect rect, QVector4D color)
{
    // flip Y to match GL orientation
    int rw = (height() - rect.y) - rect.h;

    glViewport(rect.x, rw, rect.w, rect.h);
    glScissor(rect.x, rw, rect.w, rect.h);
    glClearColor(color[0], color[1], color[2], color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void QMDLRenderer::setupWireframe()
{
    glBindTexture(GL_TEXTURE_2D, _blackTexture);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void QMDLRenderer::setupTextured()
{
    glBindTexture(GL_TEXTURE_2D, _modelTexture);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void QMDLRenderer::drawModels(const Matrix4 &projection, bool is_2d)
{
    glUniformMatrix4fv(_projectionUniformLocation, 1, false, projection.data());

    glDisable(GL_CULL_FACE);

    setupTextured();
    glBindTexture(GL_TEXTURE_2D, _whiteTexture);

    if (MainWindow::instance().showGrid())
    {
        glBindVertexArray(_gridVao);
        glBindBuffer(GL_ARRAY_BUFFER, _gridBuffer);
        glVertexAttrib4f(_colorAttributeLocation, 1.0f, 0.5f, 0.0f, 0.50f);
        glVertexAttrib2f(_texcoordAttributeLocation, 1.0f, 1.0f);
        glDrawArrays(GL_LINES, 0, _gridSize);
    }
    
    if (MainWindow::instance().showOrigin())
    {
        glDisable(GL_DEPTH_TEST);

        glBindVertexArray(_axisVao);
        glBindBuffer(GL_ARRAY_BUFFER, _axisBuffer);
        glDrawArrays(GL_LINES, 0, 6);

        glEnable(GL_DEPTH_TEST);
    }

    if (!_model)
        return;

    // model
    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _buffer);
    glVertexAttrib4f(_colorAttributeLocation, 1.0f, 1.0f, 1.0f, 1.0f);

    glEnable(GL_CULL_FACE);

    if (is_2d)
        setupWireframe();
    else
        setupTextured();

    glDrawArrays(GL_TRIANGLES, 0, _bufferData.size());
    
    if (MainWindow::instance().vertexTicks())
    {
        // points
        glPointSize(3.0f);
    
        // this is super ugly but it works ok enough for now.
        glDepthFunc(GL_LEQUAL);
	
        Matrix4 depthProj = projection;
        if (!depthProj(3, 3))
        {
            constexpr float n = 0.1;
            constexpr float f = 1024;
            constexpr float delta = 0.25f;
            constexpr float pz = 8.5f;
	        constexpr float epsilon = -2.0f * f * n * delta / ((f + n) * pz * (pz + delta));

    	    depthProj(3, 3) = -epsilon;
        }
        else
            depthProj.scale(1.0f, 1.0f, 0.98f);
        glUniformMatrix4fv(_projectionUniformLocation, 1, false, depthProj.data());

        glBindTexture(GL_TEXTURE_2D, _whiteTexture);
        glBindVertexArray(_pointVao);
        glBindBuffer(GL_ARRAY_BUFFER, _pointBuffer);
        glDrawArrays(GL_POINTS, 0, _pointData.size());

        glDepthFunc(GL_LESS);
    }
}

void QMDLRenderer::draw2D(Orientation2D orientation, QuadrantFocus quadrant)
{
    QuadRect rect = getQuadrantRect(quadrant);

    float hw = (rect.w / 2.0f);
    float hh = (rect.h / 2.0f);
        
    QMatrix4x4 projection;
    projection.ortho(-hw, hw, -hh, hh, -8192, 8192);

    QMatrix4x4 modelview;
    modelview.scale(_2dZoom);

    if (orientation == Orientation2D::XY)
    {
        modelview.translate(_2dOffset.x(), _2dOffset.y());
        modelview.rotate(-90, 0.0f, 0.0f, 1.0f);
    }
    else if (orientation == Orientation2D::XZ)
    {
        modelview.translate(_2dOffset.y(), _2dOffset.z());
        modelview.rotate(-90, 0.0f, 0.0f, 1.0f);
        modelview.rotate(-90, 0.0f, 1.0f, 0.0f);
        modelview.rotate(-90, 0.0f, 0.0f, 1.0f);
    }
    else if (orientation == Orientation2D::ZY)
    {
        modelview.translate(_2dOffset.x(), _2dOffset.z());
        modelview.rotate(-90, 0.0f, 0.0f, 1.0f);
        modelview.rotate(-90, 0.0f, 1.0f, 0.0f);
    }
        
    glUniformMatrix4fv(_modelviewUniformLocation, 1, false, modelview.data());

    clearQuadrant(rect, { 0.4f, 0.4f, 0.4f, 1.0f });

    drawModels(projection, true);
}

void QMDLRenderer::draw3D(QuadrantFocus quadrant)
{
    QuadRect rect = getQuadrantRect(quadrant);
        
    // top-right
    _camera.perspective(45, (float) rect.w / rect.h, 0.1f, 1024.f);
        
    Matrix4 modelview = _camera.getViewMatrix();
    modelview.rotate(-90, { 1, 0, 0 });
    modelview.translate(-_2dOffset.y(), _2dOffset.x(), _2dOffset.z());
    glUniformMatrix4fv(_modelviewUniformLocation, 1, false, modelview.data());

    clearQuadrant(rect, { 0.4f, 0.4f, 0.4f, 1.0f });

    drawModels(_camera.getProjectionMatrix(), false);
}
    
void QMDLRenderer::paintGL()
{
    if (_model)
        this->rebuildBuffer();

#ifdef RENDERDOC_SUPPORT
    if (_doRenderDoc && rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);
#endif

    clearQuadrant({ 0, 0, width(), height() }, { 0, 0, 0, 255 });
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
    glUseProgram(_program);
        
    draw2D(Orientation2D::XY, QuadrantFocus::TopLeft);
    draw2D(Orientation2D::ZY, QuadrantFocus::BottomLeft);
    draw2D(Orientation2D::XZ, QuadrantFocus::BottomRight);

    draw3D(QuadrantFocus::TopRight);
        
#ifdef RENDERDOC_SUPPORT
    if (_doRenderDoc && rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);
#endif

    _doRenderDoc = false;

    if (_animationTimer.isValid())
        update();
}

GLuint QMDLRenderer::createShader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
    const char *sources[] = { source };
    GLint lengths[] = { (GLint) strlen(source) };

	glShaderSource(shader, 1, sources, lengths);
	glCompileShader(shader);

    GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	if (status == GL_TRUE)
		return shader;

    static char infoLog[1024];
    static GLsizei length;
    glGetShaderInfoLog(shader, sizeof(infoLog), &length, infoLog);
	glDeleteShader(shader);
    throw std::runtime_error(infoLog);
}

GLuint QMDLRenderer::createProgram(GLuint vertexShader, GLuint fragmentShader)
{
	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

    GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status == GL_TRUE)
		return program;
        
    static char infoLog[1024];
    static GLsizei length;
    glGetProgramInfoLog(program, sizeof(infoLog), &length, infoLog);
	glDeleteProgram(program);
    throw std::runtime_error(infoLog);
}

void QMDLRenderer::rebuildBuffer()
{
    size_t count = _model->triangles.size() * 3;
    bool full_upload = _bufferData.size() < count;
    _bufferData.resize(count);
    _pointData.resize(count);
    size_t n = 0, pn = 0;
    int cur_frame, next_frame;
    float frac = 0.0f;

    if (_animationTimer.isValid())
    {
        float frame_time = ((float) _animationTimer.nsecsElapsed() / 1000000000ull) * MainWindow::instance().animationFrameRate();
        int frame_offset = (int) frame_time;

        if (MainWindow::instance().animationInterpolated())
            frac = frame_time - frame_offset;

        int start = MainWindow::instance().animationStartFrame();
        int end = MainWindow::instance().animationEndFrame();

        if (end <= start)
            cur_frame = next_frame = MainWindow::instance().activeFrame();
        else
        {
            cur_frame = start + (frame_offset % (end - start));
            next_frame = start + ((frame_offset + 1) % (end - start));
        }
    }
    else
    {
        cur_frame = next_frame = MainWindow::instance().activeFrame();
    }

    for (auto &tri : this->_model->triangles)
    {
        auto &cv0 = this->_model->frames[cur_frame].vertices[tri.vertices[0]];
        auto &cv1 = this->_model->frames[cur_frame].vertices[tri.vertices[1]];
        auto &cv2 = this->_model->frames[cur_frame].vertices[tri.vertices[2]];

        auto &nv0 = this->_model->frames[next_frame].vertices[tri.vertices[0]];
        auto &nv1 = this->_model->frames[next_frame].vertices[tri.vertices[1]];
        auto &nv2 = this->_model->frames[next_frame].vertices[tri.vertices[2]];

        std::array<QVector3D, 3> positions;
        
        if (frac == 0.0f)
        {
            positions[0] = cv0.position;
            positions[1] = cv1.position;
            positions[2] = cv2.position;
        }
        else if (frac == 1.0f)
        {
            positions[0] = nv0.position;
            positions[1] = nv1.position;
            positions[2] = nv2.position;
        }
        else
        {
            positions[0] = {
                std::lerp(cv0.position[0], nv0.position[0], frac),
                std::lerp(cv0.position[1], nv0.position[1], frac),
                std::lerp(cv0.position[2], nv0.position[2], frac)
            };
            positions[1] = {
                std::lerp(cv1.position[0], nv1.position[0], frac),
                std::lerp(cv1.position[1], nv1.position[1], frac),
                std::lerp(cv1.position[2], nv1.position[2], frac)
            };
            positions[2] = {
                std::lerp(cv2.position[0], nv2.position[0], frac),
                std::lerp(cv2.position[1], nv2.position[1], frac),
                std::lerp(cv2.position[2], nv2.position[2], frac)
            };
        }
            
        auto &st0 = this->_model->texcoords[tri.texcoords[0]];
        auto &st1 = this->_model->texcoords[tri.texcoords[1]];
        auto &st2 = this->_model->texcoords[tri.texcoords[2]];

        {
            auto &ov0 = _bufferData[n++];
            auto &ov1 = _bufferData[n++];
            auto &ov2 = _bufferData[n++];
            
            ov0.position = positions[0];
            ov1.position = positions[1];
            ov2.position = positions[2];
            
            ov0.texcoord = st0;
            ov1.texcoord = st1;
            ov2.texcoord = st2;
        }

        {
            auto &ov0 = _pointData[pn++];
            auto &ov1 = _pointData[pn++];
            auto &ov2 = _pointData[pn++];
            
            ov0.position = positions[0];
            ov1.position = positions[1];
            ov2.position = positions[2];
            
            ov0.color = ov1.color = ov2.color = { 255, 255, 255, 127 };
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, _buffer);

    if (full_upload)
        glBufferData(GL_ARRAY_BUFFER, sizeof(GPUVertexData) * count, _bufferData.data(), GL_DYNAMIC_DRAW);
    else
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GPUVertexData) * count, _bufferData.data());

    glBindBuffer(GL_ARRAY_BUFFER, _pointBuffer);

    if (full_upload)
        glBufferData(GL_ARRAY_BUFFER, sizeof(GPUPointData) * count, _pointData.data(), GL_DYNAMIC_DRAW);
    else
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GPUPointData) * count, _pointData.data());
}

void QMDLRenderer::setModel(const ModelData *model)
{
    this->_model = model;

    glBindTexture(GL_TEXTURE_2D, _modelTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, model->skins[0].width, model->skins[0].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, model->skins[0].data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //glGenerateMipmap(GL_TEXTURE_2D);

	this->update();
}

void QMDLRenderer::captureRenderDoc(bool)
{
#ifdef RENDERDOC_SUPPORT
    if (!rdoc_api)
    {
        if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
        {
            pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
            RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
        }
    }

    this->_doRenderDoc = true;
    this->update();
#endif
}

void QMDLRenderer::resetAnimation()
{
    if (_animationTimer.isValid())
    {
        _animationTimer.restart();
        update();
    }
}

void QMDLRenderer::setAnimated(bool animating)
{
    bool isAnimating = _animationTimer.isValid();

    if (isAnimating == animating)
        return;

    if (!animating)
        _animationTimer.invalidate();
    else
    {
        _animationTimer.start();
        update();
    }
}