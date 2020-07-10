//
// Copyright 2019 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "pxr/imaging/hdx/colorChannelTask.h"
#include "pxr/imaging/hdx/package.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hf/perfLog.h"
#include "pxr/imaging/hio/glslfx.h"

#include "pxr/imaging/hgi/blitCmds.h"
#include "pxr/imaging/hgi/blitCmdsOps.h"
#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((colorChannelFrag, "ColorChannelFragment"))
    (colorIn)
);

HdxColorChannelTask::HdxColorChannelTask(
    HdSceneDelegate* delegate, 
    SdfPath const& id)
    : HdxTask(id)
    , _channel(HdxColorChannelTokens->color)
{
}

HdxColorChannelTask::~HdxColorChannelTask()
{
<<<<<<< HEAD
    // #nv begin gl-errors
    bool performedGLOperation = false;
    // nv end

    if (_texture != 0) {
        glDeleteTextures(1, &_texture);
        // #nv begin gl-errors
        performedGLOperation = true;
        // nv end
    }

    if (_vertexBuffer != 0) {
        glDeleteBuffers(1, &_vertexBuffer);
        // #nv begin gl-errors
        performedGLOperation = true;
        // nv end
    }

    if (_shaderProgram) {
        _shaderProgram.reset();
    }

    if (_copyFramebuffer != 0) {
        glDeleteFramebuffers(1, &_copyFramebuffer);
        // #nv begin gl-errors
        performedGLOperation = true;
        // nv end
    }

    // #nv begin gl-errors
    if (performedGLOperation) {
        GLF_POST_PENDING_GL_ERRORS();
    }
    // nv end
}

bool
HdxColorChannelTask::_CreateShaderResources()
{
    if (_shaderProgram) {
        return true;
    }

    _shaderProgram.reset(new HdStGLSLProgram(_tokens->colorChannelShader));

    HioGlslfx glslfx(HdxPackageColorChannelShader());

    std::string fragCode = glslfx.GetSource(_tokens->colorChannelFragment);

    if (!_shaderProgram->CompileShader(GL_VERTEX_SHADER,
            glslfx.GetSource(_tokens->colorChannelVertex)) ||
        !_shaderProgram->CompileShader(GL_FRAGMENT_SHADER, fragCode) ||
        !_shaderProgram->Link()) {
        TF_CODING_ERROR("Failed to load color channel shader");
        _shaderProgram.reset();
        return false;
    }

    GLuint programId = _shaderProgram->GetProgram().GetId();
    _locations[COLOR_IN] = glGetUniformLocation(programId, "colorIn");
    _locations[POSITION] = glGetAttribLocation(programId, "position");
    _locations[UV_IN]    = glGetAttribLocation(programId, "uvIn");
    _locations[CHANNEL]  = glGetUniformLocation(programId, "channel");
    
    GLF_POST_PENDING_GL_ERRORS();
    return true;
}

bool
HdxColorChannelTask::_CreateBufferResources()
{
    if (_vertexBuffer) {
        return true;
    }

    // A larger-than screen triangle with UVs made to fit the screen.
    //                                 positions          |   uvs
    static const float vertices[] = { -1,  3, -1, 1,        0, 2,
                                      -1, -1, -1, 1,        0, 0,
                                       3, -1, -1, 1,        2, 0 };

    glGenBuffers(1, &_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices),
                 &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLF_POST_PENDING_GL_ERRORS();
    return true;
}

void
HdxColorChannelTask::_CopyTexture()
{
    GLint restoreReadFB, restoreDrawFB;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &restoreReadFB);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &restoreDrawFB);

    // Make a copy of the default FB color attachment so we can read from the
    // copy and write back into it.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, restoreDrawFB);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _copyFramebuffer);

    int width = _textureSize[0];
    int height = _textureSize[1];

    glBlitFramebuffer(0, 0, width, height,
                      0, 0, width, height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, restoreReadFB);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, restoreDrawFB);

    GLF_POST_PENDING_GL_ERRORS();
}

bool
HdxColorChannelTask::_CreateFramebufferResources()
{
    // If framebufferSize is not provided we use the viewport size.
    // This can be incorrect if the client/app has changed the viewport to
    // be different then the render window size. (E.g. UsdView CameraMask mode)
    GfVec2i fboSize = _framebufferSize;
    if (fboSize[0] <= 0 || fboSize[1] <= 0) {
        GLint res[4] = {0};
        glGetIntegerv(GL_VIEWPORT, res);
        fboSize = GfVec2i(res[2], res[3]);
        _framebufferSize = fboSize;
    }

    bool createTexture = (_texture == 0 || fboSize != _textureSize);

    if (createTexture) {
        if (_texture != 0) {
            glDeleteTextures(1, &_texture);
            _texture = 0;
        }

        _textureSize = fboSize;

        GLint restoreTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &restoreTexture);

        glGenTextures(1, &_texture);
        glBindTexture(GL_TEXTURE_2D, _texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // XXX For now we assume we always want R16F. We could perhaps expose
        //     this as client-API in HdxColorChannelTaskParams.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, _textureSize[0], 
                     _textureSize[1], 0, GL_RGBA, GL_FLOAT, 0);

        glBindTexture(GL_TEXTURE_2D, restoreTexture);
    }

    if (_copyFramebuffer == 0) {
        glGenFramebuffers(1, &_copyFramebuffer);
    }

    if (createTexture) {
        GLint restoreReadFB, restoreDrawFB;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &restoreReadFB);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &restoreDrawFB);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _copyFramebuffer);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                               GL_TEXTURE_2D, _texture, 0);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, restoreReadFB);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, restoreDrawFB);
    }

    GLF_POST_PENDING_GL_ERRORS();
    return true;
}

void
HdxColorChannelTask::_ApplyColorChannel()
{
    // A note here: colorChannel is used for all of our plugins and has to be
    // robust to poor GL support.  OSX compatibility profile provides a
    // GL 2.1 API, slightly restricting our choice of API and heavily
    // restricting our shader syntax. See also HdxFullscreenShader.

    // Read from the texture-copy we made of the clients FBO and output the
    // color-corrected pixels into the clients FBO.

    GLuint programId = _shaderProgram->GetProgram().GetId();
    glUseProgram(programId);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture);
    glUniform1i(_locations[COLOR_IN], 0);

    glUniform1i(_locations[CHANNEL], _GetChannelAsGLint());

    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
    glVertexAttribPointer(_locations[POSITION], 4, GL_FLOAT, GL_FALSE,
                          sizeof(float)*6, 0);
    glEnableVertexAttribArray(_locations[POSITION]);
    glVertexAttribPointer(_locations[UV_IN], 2, GL_FLOAT, GL_FALSE,
            sizeof(float)*6, reinterpret_cast<void*>(sizeof(float)*4));
    glEnableVertexAttribArray(_locations[UV_IN]);

    // We are rendering a full-screen triangle, which would render to depth.
    // Instead we want to preserve the original depth, so disable depth writes.
    GLboolean restoreDepthWriteMask;
    GLboolean restoreStencilWriteMask;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &restoreDepthWriteMask);
    glGetBooleanv(GL_STENCIL_WRITEMASK, &restoreStencilWriteMask);
    glDepthMask(GL_FALSE);
    glStencilMask(GL_FALSE);

    // Depth test must be ALWAYS instead of disabling the depth_test because
    // we still want to write to the depth buffer. Disabling depth_test disables
    // depth_buffer writes.
    GLint restoreDepthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &restoreDepthFunc);
    glDepthFunc(GL_ALWAYS);

    GLint restoreViewport[4] = {0};
    glGetIntegerv(GL_VIEWPORT, restoreViewport);
    glViewport(0, 0, _framebufferSize[0], _framebufferSize[1]);

    // The app may have alpha blending enabled.
    // We want to pass-through the alpha values, not alpha-blend on top of dest.
    GLboolean restoreblendEnabled;
    glGetBooleanv(GL_BLEND, &restoreblendEnabled);
    glDisable(GL_BLEND);

    // Alpha to coverage would prevent any pixels that have an alpha of 0.0 from
    // being written. We want to color correct all pixels. Even background
    // pixels that were set with a clearColor alpha of 0.0
    GLboolean restoreAlphaToCoverage;
    glGetBooleanv(GL_SAMPLE_ALPHA_TO_COVERAGE, &restoreAlphaToCoverage);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (restoreAlphaToCoverage) {
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }

    if (restoreblendEnabled) {
        glEnable(GL_BLEND);
    }

    glViewport(restoreViewport[0], restoreViewport[1],
               restoreViewport[2], restoreViewport[3]);

    glDepthFunc(restoreDepthFunc);
    glDepthMask(restoreDepthWriteMask);
    glStencilMask(restoreStencilWriteMask);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(_locations[POSITION]);
    glDisableVertexAttribArray(_locations[UV_IN]);

    glUseProgram(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLF_POST_PENDING_GL_ERRORS();
}

GLint
HdxColorChannelTask::_GetChannelAsGLint()
{
    // Return the index of the current channel in HdxColorChannelTokens
    int i = 0;
    for(const TfToken& channelToken : HdxColorChannelTokens->allTokens) {
        if (channelToken == _channel) return i;
        ++i;
=======
    if (_parameterBuffer) {
        _GetHgi()->DestroyBuffer(&_parameterBuffer);
>>>>>>> v20.08-rc1
    }
}

void
HdxColorChannelTask::_Sync(HdSceneDelegate* delegate,
                           HdTaskContext* ctx,
                           HdDirtyBits* dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (!_compositor) {
        _compositor.reset(new HdxFullscreenShader(_GetHgi(), "ColorChannel"));
    }

    if ((*dirtyBits) & HdChangeTracker::DirtyParams) {
        HdxColorChannelTaskParams params;

        if (_GetTaskParams(delegate, &params)) {
            _channel = params.channel;
        }
    }

    *dirtyBits = HdChangeTracker::Clean;
}

void
HdxColorChannelTask::Prepare(HdTaskContext* ctx,
                                HdRenderIndex* renderIndex)
{
}

void
HdxColorChannelTask::Execute(HdTaskContext* ctx)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    HgiTextureHandle aovTexture;
    _GetTaskContextData(ctx, HdAovTokens->color, &aovTexture);

    _compositor->SetProgram(
        HdxPackageColorChannelShader(), 
        _tokens->colorChannelFrag);

    _CreateParameterBuffer();
    _compositor->BindBuffer(_parameterBuffer, 0);

    _compositor->BindTextures(
        {_tokens->colorIn}, 
        {aovTexture});

    _compositor->Draw(aovTexture, /*no depth*/HgiTextureHandle());
}

void
HdxColorChannelTask::_CreateParameterBuffer()
{
    _ParameterBuffer pb;

    // Get an integer that represents the color channel. This can be used to
    // pass the color channel option as into the glsl shader.
    // (see the `#define CHANNEL_*` lines in the shader). 
    // If _channel contains an invalid entry the shader will return 'color'.
    int i = 0;
    for(const TfToken& channelToken : HdxColorChannelTokens->allTokens) {
        if (channelToken == _channel) break;
        ++i;
    }
    pb.channel = i;

    // All data is still the same, no need to update the storage buffer
    if (_parameterBuffer && pb == _parameterData) {
        return;
    }
    _parameterData = pb;

    if (!_parameterBuffer) {
        // Create a new (storage) buffer for shader parameters
        HgiBufferDesc bufDesc;
        bufDesc.debugName = "HdxColorChannelTask parameter buffer";
        bufDesc.usage = HgiBufferUsageStorage;
        bufDesc.initialData = &_parameterData;
        bufDesc.byteSize = sizeof(_parameterData);
        _parameterBuffer = _GetHgi()->CreateBuffer(bufDesc);
    } else {
        // Update the existing storage buffer with new values.
        HgiBlitCmdsUniquePtr blitCmds = _GetHgi()->CreateBlitCmds();
        HgiBufferCpuToGpuOp copyOp;
        copyOp.byteSize = sizeof(_parameterData);
        copyOp.cpuSourceBuffer = &_parameterData;
        copyOp.sourceByteOffset = 0;
        copyOp.destinationByteOffset = 0;
        copyOp.gpuDestinationBuffer = _parameterBuffer;
        blitCmds->CopyBufferCpuToGpu(copyOp);
        _GetHgi()->SubmitCmds(blitCmds.get());
    }
}


// -------------------------------------------------------------------------- //
// VtValue Requirements
// -------------------------------------------------------------------------- //

std::ostream& operator<<(
    std::ostream& out, 
    const HdxColorChannelTaskParams& pv)
{
    out << "ColorChannelTask Params: (...) "
        << pv.channel << " "
    ;
    return out;
}

bool operator==(const HdxColorChannelTaskParams& lhs,
                const HdxColorChannelTaskParams& rhs)
{
    return lhs.channel == rhs.channel;
}

bool operator!=(const HdxColorChannelTaskParams& lhs,
                const HdxColorChannelTaskParams& rhs)
{
    return !(lhs == rhs);
}

PXR_NAMESPACE_CLOSE_SCOPE
