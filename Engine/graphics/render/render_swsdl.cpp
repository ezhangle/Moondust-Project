/*
 * Platformer Game Engine by Wohlstand, a free platform for game making
 * Copyright (c) 2016 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <SDL2/SDL_rect.h>
#include "render_swsdl.h"

#include "../window.h"
#include <common_features/graphics_funcs.h>
#include <common_features/logger.h>
#include <common_features/maths.h>
#include <cmath>

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL2/SDL.h> // SDL 2 Library

#include "../gl_debug.h"

#ifdef _WIN32
#define FREEIMAGE_LIB
#endif
#include <FreeImageLite.h>


Render_SW_SDL::Render_SW_SDL() : Render_Base("Software SDL"),
    m_gRenderer(NULL),
    m_clearColor{0, 0, 0, 0},
    m_currentTexture(NULL),
    //Virtual resolution of renderable zone
    window_w(800.0f),
    window_h(600.0f),
    //Scale of virtual and window resolutuins
    scale_x(1.0f),
    scale_y(1.0f),
    //Side offsets to keep ratio
    offset_x(0.0f),
    offset_y(0.0f),
    //current viewport
    viewport_x(0.0f),
    viewport_y(0.0f),
    //Need to calculate relative viewport position when screen was scaled
    viewport_scale_x(1.0f),
    viewport_scale_y(1.0f),
    //Resolution of viewport
    viewport_w(800.0f),
    viewport_h(600.0f),
    //Half values of viewport Resolution
    viewport_w_half(400.0f),
    viewport_h_half(300.0f),
    //Texture render color levels
    color_binded_texture{1.0f, 1.0f, 1.0f, 1.0f}
{
    m_textureBank.push_back(NULL);
}

Render_SW_SDL::~Render_SW_SDL()
{}

void Render_SW_SDL::set_SDL_settings()
{
    SDL_GL_ResetAttributes();
}

bool Render_SW_SDL::init()
{
    //Initialize clear color
    setClearColor(0.f, 0.f, 0.f, 1.f);
    //Create renderer for window
    m_gRenderer = SDL_CreateRenderer(PGE_Window::window, -1, SDL_RENDERER_SOFTWARE);

    if(!m_gRenderer)
    {
        pLogWarning("SW SDL: Failed to initialize screen surface! %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawBlendMode(m_gRenderer, SDL_BLENDMODE_BLEND);
    return true;
}

bool Render_SW_SDL::uninit()
{
    deleteTexture(_dummyTexture);
    SDL_DestroyRenderer(m_gRenderer);
    return true;
}

void Render_SW_SDL::initDummyTexture()
{
    FIBITMAP *image = GraphicsHelps::loadImageRC("_broken.png");

    if(!image)
    {
        std::string msg = QString("Can't initialize dummy texture!\n"
                                  "In file: %1:%2").arg(__FILE__).arg(__LINE__).toStdString();
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                                 "OpenGL Error", msg.c_str(), NULL);
        abort();
    }

    int w = static_cast<int>(FreeImage_GetWidth(image));
    int h = static_cast<int>(FreeImage_GetHeight(image));
    _dummyTexture.nOfColors = GL_RGBA;
    _dummyTexture.format = GL_BGRA;
    _dummyTexture.w = w;
    _dummyTexture.h = h;
    GLubyte *textura = reinterpret_cast<GLubyte *>(FreeImage_GetBits(image));
    loadTexture(_dummyTexture, w, h, textura);
    GraphicsHelps::closeImage(image);
}

PGE_Texture Render_SW_SDL::getDummyTexture()
{
    return _dummyTexture;
}

void Render_SW_SDL::loadTexture(PGE_Texture &target, int width, int height, unsigned char *RGBApixels)
{
    SDL_Surface *surface;
    SDL_Texture *texture;
    surface = SDL_CreateRGBSurfaceFrom(RGBApixels, width, height, 32, width * 4,
                                       FI_RGBA_RED_MASK,
                                       FI_RGBA_GREEN_MASK,
                                       FI_RGBA_BLUE_MASK,
                                       FI_RGBA_ALPHA_MASK);
    texture = SDL_CreateTextureFromSurface(m_gRenderer, surface);
    SDL_FreeSurface(surface);
checkStackAgain:

    if(!m_textureFreeNumbers.empty())
    {
        GLuint textureID;

        do  //Fetch one of free texture numbers
        {
            textureID = m_textureFreeNumbers.top();
            m_textureFreeNumbers.pop();
            //Repeat if texture ID is useless: if id is larger than texture bank
        }
        while(!m_textureFreeNumbers.empty() && (textureID >= m_textureBank.size()));

        //If texture still useles,
        if(m_textureFreeNumbers.empty() && (textureID >= m_textureBank.size()))
            goto checkStackAgain;//Push new texture to the tail of the texture bank

        target.texture = textureID;
        m_textureBank[ textureID ] = texture;
    }
    else
    {
        //Push new texture to the tail of the texture bank
        target.texture = static_cast<GLuint>(m_textureBank.size());
        m_textureBank.push_back(texture);
    }

    target.inited = true;
}

void Render_SW_SDL::deleteTexture(PGE_Texture &tx)
{
    if(tx.texture >= m_textureBank.size())
    {
        tx.inited = false;
        return;
    }

    SDL_Texture *corpse = m_textureBank[tx.texture];
    SDL_DestroyTexture(corpse);

    if(tx.texture != (m_textureBank.size() - 1))
    {
        //If entry deleted from middle, remember that number to use it again
        m_textureFreeNumbers.push(tx.texture);
        m_textureBank[ tx.texture ] = NULL;
    }
    else
    {
        //If entry deleted from tail, delete it and all null elements between next not-null
        do
        {
            m_textureBank.pop_back();
        }
        while((m_textureBank.size() != 1) && (m_textureBank.back() == NULL));
    }

    tx.texture = 0;
    tx.inited = false;
}

bool Render_SW_SDL::isTopDown()
{
    return false;
}

void Render_SW_SDL::getScreenPixels(int x, int y, int w, int h, unsigned char *pixels)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_RenderReadPixels(m_gRenderer,
                         &rect,
                         SDL_PIXELFORMAT_BGR24,
                         pixels,
                         w * 3 + (w % 4));
}

void Render_SW_SDL::getScreenPixelsRGBA(int x, int y, int w, int h, unsigned char *pixels)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_RenderReadPixels(m_gRenderer,
                         &rect,
                         SDL_PIXELFORMAT_ABGR8888,
                         pixels,
                         w * 4);
}

void Render_SW_SDL::setViewport(int x, int y, int w, int h)
{
    float xF = static_cast<float>(x);
    float yF = static_cast<float>(y);
    float wF = static_cast<float>(w);
    float hF = static_cast<float>(h);
    SDL_Rect topLeftViewport;
    topLeftViewport.x = Maths::iRound(offset_x + std::ceil(xF * viewport_scale_x));
    topLeftViewport.y = Maths::iRound(offset_y + std::ceil(yF * viewport_scale_y));
    topLeftViewport.w = Maths::iRound(wF * viewport_scale_x);
    topLeftViewport.h = Maths::iRound(hF * viewport_scale_y);
    SDL_RenderSetViewport(m_gRenderer, &topLeftViewport);
    viewport_x = xF;
    viewport_y = yF;
    setViewportSize(wF, hF);
}

void Render_SW_SDL::resetViewport()
{
    float w, w1, h, h1;
    int   wi, hi;
    SDL_GetWindowSize(PGE_Window::window, &wi, &hi);
    w = wi;
    h = hi;
    w1 = w;
    h1 = h;
    scale_x = w / window_w;
    scale_y = h / window_h;
    viewport_scale_x = scale_x;
    viewport_scale_y = scale_y;

    if(scale_x > scale_y)
    {
        w1 = scale_y * window_w;
        viewport_scale_x = w1 / window_w;
    }
    else if(scale_x < scale_y)
    {
        h1 = scale_x * window_h;
        viewport_scale_y = h1 / window_h;
    }

    offset_x = (w - w1) / 2;
    offset_y = (h - h1) / 2;
    //glViewport(offset_x, offset_y, (GLsizei)w1, (GLsizei)h1); GLERRORCHECK();
    SDL_Rect topLeftViewport;
    topLeftViewport.x = static_cast<int>(offset_x);
    topLeftViewport.y = static_cast<int>(offset_y);
    topLeftViewport.w = static_cast<int>(w1);
    topLeftViewport.h = static_cast<int>(h1);
    SDL_RenderSetViewport(m_gRenderer, &topLeftViewport);
    setViewportSize(window_w, window_h);
}

void Render_SW_SDL::setViewportSize(int w, int h)
{
    setViewportSize(static_cast<float>(w), static_cast<float>(h));
}

void Render_SW_SDL::setViewportSize(float w, float h)
{
    viewport_w = w;
    viewport_h = h;
    viewport_w_half = w / 2.0f;
    viewport_h_half = h / 2.0f;
}

void Render_SW_SDL::setWindowSize(int w, int h)
{
    window_w = w;
    window_h = h;
    resetViewport();
}

void Render_SW_SDL::flush()
{}

void Render_SW_SDL::repaint()
{
    SDL_RenderPresent(m_gRenderer);
}

void Render_SW_SDL::setClearColor(float r, float g, float b, float a)
{
    m_clearColor[0] = static_cast<unsigned char>(255.f * r);
    m_clearColor[1] = static_cast<unsigned char>(255.f * g);
    m_clearColor[2] = static_cast<unsigned char>(255.f * b);
    m_clearColor[3] = static_cast<unsigned char>(255.f * a);
}

void Render_SW_SDL::clearScreen()
{
    SDL_SetRenderDrawColor(m_gRenderer, m_clearColor[0],
                           m_clearColor[1],
                           m_clearColor[2],
                           m_clearColor[3]);
    SDL_RenderClear(m_gRenderer);
}

void Render_SW_SDL::getPixelData(const PGE_Texture *tx, unsigned char *pixelData)
{
    if(!tx) return;

    setRenderTexture(const_cast<PGE_Texture *>(tx)->texture);
    int pitch, w, h, a;
    void *pixels;
    SDL_SetTextureBlendMode(m_currentTexture, SDL_BLENDMODE_BLEND);
    SDL_QueryTexture(m_currentTexture, NULL, &a, &w, &h);
    SDL_LockTexture(m_currentTexture, NULL, &pixels, &pitch);
    memcpy(pixelData, pixels, static_cast<size_t>(pitch * h));
    SDL_UnlockTexture(m_currentTexture);
    setUnbindTexture();
}

void Render_SW_SDL::renderRect(float x, float y, float w, float h, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha, bool filled)
{
    SDL_Rect aRect = scaledRect(x, y, w, h);
    SDL_SetRenderDrawColor(m_gRenderer,
                           static_cast<unsigned char>(255.f * red),
                           static_cast<unsigned char>(255.f * green),
                           static_cast<unsigned char>(255.f * blue),
                           static_cast<unsigned char>(255.f * alpha)
                          );

    if(filled)
        SDL_RenderFillRect(m_gRenderer, &aRect);
    else
        SDL_RenderDrawRect(m_gRenderer, &aRect);
}

void Render_SW_SDL::renderRectBR(float _left, float _top, float _right, float _bottom, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    SDL_Rect aRect = scaledRectS(_left, _top, _right, _bottom);
    SDL_SetRenderDrawColor(m_gRenderer,
                           static_cast<unsigned char>(255.f * red),
                           static_cast<unsigned char>(255.f * green),
                           static_cast<unsigned char>(255.f * blue),
                           static_cast<unsigned char>(255.f * alpha)
                          );
    SDL_RenderFillRect(m_gRenderer, &aRect);
}

void Render_SW_SDL::renderTexture(PGE_Texture *texture, float x, float y)
{
    if(!texture) return;

    setRenderTexture(texture->texture);
    m_currentTextureRect.setRect(0, 0, texture->w, texture->h);

    if(!m_currentTexture)
    {
        renderRect(x, y, texture->w, texture->h,
                   color_binded_texture[0],
                   color_binded_texture[1],
                   color_binded_texture[2],
                   color_binded_texture[3]);
        return;
    }

    SDL_Rect aRect = scaledRectIS(x, y, texture->w, texture->h);
    SDL_SetTextureColorMod(m_currentTexture,
                           static_cast<unsigned char>(255.f * color_binded_texture[0]),
                           static_cast<unsigned char>(255.f * color_binded_texture[1]),
                           static_cast<unsigned char>(255.f * color_binded_texture[2]));
    SDL_SetTextureAlphaMod(m_currentTexture, static_cast<unsigned char>(255.f * color_binded_texture[3]));
    SDL_RenderCopy(m_gRenderer, m_currentTexture, NULL, &aRect);
    setUnbindTexture();
}

void Render_SW_SDL::renderTexture(PGE_Texture *texture, float x, float y, float w, float h, float ani_top, float ani_bottom, float ani_left, float ani_right)
{
    if(!texture) return;

    setRenderTexture(texture->texture);
    m_currentTextureRect.setRect(0, 0, texture->w, texture->h);

    if(!m_currentTexture)
    {
        renderRect(x, y, w, h,
                   static_cast<unsigned char>(255.f * color_binded_texture[0]),
                   static_cast<unsigned char>(255.f * color_binded_texture[1]),
                   static_cast<unsigned char>(255.f * color_binded_texture[2]),
                   static_cast<unsigned char>(255.f * color_binded_texture[3]));
        return;
    }

    float texW = static_cast<float>(texture->w);
    float texH = static_cast<float>(texture->h);
    SDL_Rect sourceRect =
    {
        Maths::iRound(texW * ani_left),
        Maths::iRound(texH * ani_top),
        Maths::iRound((texW * ani_right) - (texW * ani_left)),
        Maths::iRound((texH * ani_bottom) - (texH * ani_top))
    };
    SDL_Rect destRect = scaledRect(x, y, w, h);
    SDL_SetTextureColorMod(m_currentTexture,
                           static_cast<unsigned char>(255.f * color_binded_texture[0]),
                           static_cast<unsigned char>(255.f * color_binded_texture[1]),
                           static_cast<unsigned char>(255.f * color_binded_texture[2]));
    SDL_SetTextureAlphaMod(m_currentTexture, static_cast<unsigned char>(255.f * color_binded_texture[3]));
    SDL_RenderCopy(m_gRenderer, m_currentTexture, &sourceRect, &destRect);
    setUnbindTexture();
}

void Render_SW_SDL::renderTextureCur(float x, float y, float w, float h, float ani_top, float ani_bottom, float ani_left, float ani_right)
{
    if(!m_currentTexture)
    {
        renderRect(x, y, w, h,
                   static_cast<unsigned char>(255.f * color_binded_texture[0]),
                   static_cast<unsigned char>(255.f * color_binded_texture[1]),
                   static_cast<unsigned char>(255.f * color_binded_texture[2]),
                   static_cast<unsigned char>(255.f * color_binded_texture[3]));
        return;
    }

    float texW = static_cast<float>(m_currentTextureRect.width());
    float texH = static_cast<float>(m_currentTextureRect.height());
    SDL_Rect sourceRect =
    {
        Maths::iRound(texW * ani_left),
        Maths::iRound(texH * ani_top),
        abs(Maths::iRound((texW * ani_right) - (texW * ani_left))),
        abs(Maths::iRound((texH * ani_bottom) - (texH * ani_top)))
    };
    SDL_Rect destRect = scaledRect(x, y, w, h);
    SDL_SetTextureColorMod(m_currentTexture,
                           static_cast<unsigned char>(255.f * color_binded_texture[0]),
                           static_cast<unsigned char>(255.f * color_binded_texture[1]),
                           static_cast<unsigned char>(255.f * color_binded_texture[2]));
    SDL_SetTextureAlphaMod(m_currentTexture, static_cast<unsigned char>(255.f * color_binded_texture[3]));
    SDL_RenderCopy(m_gRenderer, m_currentTexture, &sourceRect, &destRect);
}



void Render_SW_SDL::BindTexture(PGE_Texture *texture)
{
    setRenderTexture(texture->texture);
    m_currentTextureRect.setRect(0, 0, texture->w, texture->h);
}

void Render_SW_SDL::setTextureColor(float Red, float Green, float Blue, float Alpha)
{
    color_binded_texture[0] = Red;
    color_binded_texture[1] = Green;
    color_binded_texture[2] = Blue;
    color_binded_texture[3] = Alpha;
}

void Render_SW_SDL::UnBindTexture()
{
    setUnbindTexture();
}


SDL_Rect Render_SW_SDL::scaledRectIS(float x, float y, int w, int h)
{
    return
    {
        static_cast<int>(std::ceil(x * viewport_scale_x)),
        static_cast<int>(std::ceil(y * viewport_scale_y)),
        static_cast<int>(std::ceil(static_cast<float>(w) * viewport_scale_x)),
        static_cast<int>(std::ceil(static_cast<float>(h) * viewport_scale_y))
    };
}


SDL_Rect Render_SW_SDL::scaledRect(float x, float y, float w, float h)
{
    return
    {
        static_cast<int>(std::ceil(x * viewport_scale_x)),
        static_cast<int>(std::ceil(y * viewport_scale_y)),
        static_cast<int>(std::ceil(w * viewport_scale_x)),
        static_cast<int>(std::ceil(h * viewport_scale_y))
    };
}

SDL_Rect Render_SW_SDL::scaledRectS(float left, float top, float right, float bottom)
{
    return
    {
        static_cast<int>(std::ceil(left * viewport_scale_x)),
        static_cast<int>(std::ceil(top * viewport_scale_y)),
        static_cast<int>(std::ceil((right - left)*viewport_scale_x)),
        static_cast<int>(std::ceil((bottom - top)*viewport_scale_y))
    };
}

PGE_Point Render_SW_SDL::MapToScr(PGE_Point point)
{
    return MapToScr(point.x(), point.y());
}

PGE_Point Render_SW_SDL::MapToScr(int x, int y)
{
    return PGE_Point(
               static_cast<int>((static_cast<float>(x) / viewport_scale_x) - offset_x),
               static_cast<int>((static_cast<float>(y) / viewport_scale_y) - offset_y)
           );
}

int Render_SW_SDL::alignToCenter(int x, int w)
{
    return x + (static_cast<int>(viewport_w_half) - (w / 2));
}
