/*
 * Platformer Game Engine by Wohlstand, a free platform for game making
 * Copyright (c) 2017 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "bg_multilayer.h"
#include <data_configs/config_manager.h>
#include <common_features/logger.h>
#include <graphics/gl_renderer.h>
#include <Utils/maths.h>

#include <algorithm>

MultilayerBackground::~MultilayerBackground()
{
    m_scrollers.clear();
    m_layers_back.clear();
    m_layers_front.clear();
}

void MultilayerBackground::init(const obj_BG &bg)
{
    if(!bg.setup.multi_layered || bg.setup.layers.empty())
        return;
    if(m_isInitialized)
        return;

    m_layers_back.clear();
    m_layers_front.clear();
    m_scrollers.clear();

    uint32_t layerIdCounter = 0;
    for(const BgSetup::BgLayer &layer : bg.setup.layers)
    {
        Layer l;
        l.setup = layer;
        l.autoscroll_x = layer.auto_scrolling_x;
        l.autoscroll_y = layer.auto_scrolling_y;
        l.layerId = layerIdCounter++;

        int tID = ConfigManager::getBGLayerTexture(bg.setup.id, l.layerId);
        if(tID >= 0)
        {
            l.texture = ConfigManager::level_textures[tID];
            l.tID = bg.texturePerLayer[l.layerId];
        }
        else
        {
            pLogWarning("Fail to load texture for background layer: %s", l.setup.image.c_str());
            break;
        }

        // TODO: Implement here actual loading of texture and loading properties per every layer

        if(layer.z_index <= 0)      //Background layer
            m_layers_back.push_back(LayerPtr(new Layer(l)));
        else if(layer.z_index > 0)  //Foreground layer
            m_layers_front.push_back(LayerPtr(new Layer(l)));
    }

    initScrollers(m_layers_back);
    initScrollers(m_layers_front);

    m_isInitialized = true;
}

void MultilayerBackground::initScrollers(MultilayerBackground::LayersList &ls)
{
    struct LessThanZ
    {
        inline bool operator() (const LayerPtr& struct1, const LayerPtr& struct2)
        {
            return (struct1->setup.z_index < struct2->setup.z_index);
        }
    };

    //Sort by Z-index
    std::sort(ls.begin(),  ls.end(), LessThanZ());

    //Add pointers to every layer into the scroller
    for(LayerPtr &layer_ptr : ls)
    {
        Layer &layer = *layer_ptr;
        if(layer.autoscroll_x || layer.autoscroll_y)
        {
            Scroller s;
            s.layer = layer_ptr;
            s.speed_x = layer.autoscroll_x ? layer.setup.auto_scrolling_x_speed : 0.0;
            s.speed_y = layer.autoscroll_y ? layer.setup.auto_scrolling_y_speed : 0.0;
            m_scrollers.push_back(s);
        }
    }
}

void MultilayerBackground::process(double tickDelay)
{
    for(Scroller &scroller : m_scrollers)
    {
        Layer &layer = *scroller.layer;
        //Iterate scrolling
        layer.autoscroll_x_offset += scroller.speed_x * (tickDelay / 1000.0);
        layer.autoscroll_y_offset += scroller.speed_y * (tickDelay / 1000.0);

        //Return offset to initial position with small difference when it reaches width or height of layer image
        double w = (layer.texture.w        + layer.setup.padding_x_left + layer.setup.padding_x_right) * layer.setup.parallax_coefficient_x;
        double h = (layer.texture.frame_h  + layer.setup.padding_y_top + layer.setup.padding_y_bottom) * layer.setup.parallax_coefficient_y;

        if(layer.autoscroll_y_offset > h)
            layer.autoscroll_y_offset -= h;
        else if(layer.autoscroll_y_offset < 0.0)
            layer.autoscroll_y_offset += h;

        if(layer.autoscroll_x_offset > w)
            layer.autoscroll_x_offset -= w;
        else if(layer.autoscroll_x_offset < 0.0)
            layer.autoscroll_x_offset += w;
        //(scroller.layer->autoscroll_y_offset > scroller.layer->texture.frame_h)
    }
}

void MultilayerBackground::renderBackground(const PGE_RectF &box, double x, double y, double w, double h)
{
    renderLayersList(m_layers_back, box, x, y, w, h);
}

void MultilayerBackground::renderForeground(const PGE_RectF &box, double x, double y, double w, double h)
{
    renderLayersList(m_layers_front, box, x, y, w, h);
}

void MultilayerBackground::renderLayersList(const MultilayerBackground::LayersList &layers, const PGE_RectF &box, double x, double y, double w, double h)
{
    if(!m_isInitialized)
        return;

    // TODO: Implement support for all INI values for multi-layer backgrounds:
    /*
     * padding - whitespace between every repeating layer
     * offset  - position offset of referrence point
     * auto-parallax - automatically calculate parallax coefficients per every layer in dependence on Z-Value
     * auto-scrolling - increase/decreaze offset value together with a time
     */

    for(const LayerPtr &layer_ptr : layers)
    {
        Layer   &layer    = *layer_ptr;
        const double  sWidth    = box.width();
        const double  sHeight   = box.height();
        const double  fWidth     = static_cast<double>(layer.texture.frame_w) + layer.setup.padding_x_right + layer.setup.padding_x_left;
        const double  fHeight    = static_cast<double>(layer.texture.frame_h) + layer.setup.padding_y_bottom + layer.setup.padding_y_top;

        double      pointX = x;
        double      pointY = y;

        double      imgPos_X  = 0;
        double      imgPos_Y  = 0;

        double      refPointX = 0.0;
        double      refPointY = 0.0;

        double      offsetXpre = layer.setup.offset_x + layer.autoscroll_x_offset;
        double      offsetYpre = layer.setup.offset_y + layer.autoscroll_y_offset;

        double      offsetXpost = 0.0;
        double      offsetYpost = 0.0;


        switch(layer.setup.parallax_mode_x)
        {
        // Proportionally move sprite with camera's position inside section
        case BgSetup::BgLayer::P_MODE_FIT:
            //If image width less than screen or larger than section - act as Fixed
            if((fWidth < w) || (fWidth > sWidth))
                goto bgSetupFixedW;
            else
            {
                imgPos_X = (box.left() - pointX) / ((sWidth - w) / (fWidth - w));
                if(imgPos_X > 0.0)
                    imgPos_X = 0.0;
            }

            break;
        // Scroll backround with divided offset at reference point edge
        case BgSetup::BgLayer::P_MODE_SCROLL:

            switch(layer.setup.reference_point_x)
            {
            case BgSetup::BgLayer::R_LEFT:
                refPointX = box.left() - pointX + offsetXpre;
                break;
            case BgSetup::BgLayer::R_RIGHT:
                refPointX = (box.right() - w) - pointX + offsetXpre;
                if(layer.setup.repeat_x)
                    offsetXpost = (w - fWidth) - (w - fmod(w, fWidth));
                else
                    offsetXpost = (w - fWidth);
                break;
            }
            if(layer.setup.repeat_x)
            {
                //If referrence point is positive (for example, autoscrolling have moved left), offset it
                if(refPointX > 0.0)
                    refPointX = -( (fWidth * layer.setup.parallax_coefficient_x)
                                    - std::fmod(refPointX, fWidth * layer.setup.parallax_coefficient_x) );
                imgPos_X = std::fmod(refPointX / layer.setup.parallax_coefficient_x, fWidth) + offsetXpost;
            }
            else
            {
                imgPos_X = (refPointX / layer.setup.parallax_coefficient_x) + offsetXpost;
            }
            break;

        // Fixed position
        case BgSetup::BgLayer::P_MODE_FIXED:
        bgSetupFixedW:
            switch(layer.setup.reference_point_x)
            {
            case BgSetup::BgLayer::R_LEFT:
                imgPos_X = offsetXpre;
                break;
            case BgSetup::BgLayer::R_RIGHT:
                imgPos_X = offsetXpre + (w - fWidth);
                break;
            }

            if(layer.setup.repeat_x && (imgPos_X > 0.0))
                imgPos_X = -( (fWidth) - std::fmod(imgPos_X, fWidth) );
            break;
        }


        switch(layer.setup.parallax_mode_y)
        {
        // Proportionally move sprite with camera's position inside section
        case BgSetup::BgLayer::P_MODE_FIT:
            //If image width less than screen or larger than section - act as Fixed
            if((fHeight < h) || (fHeight > sHeight))
                goto bgSetupFixedH;
            else
            {
                imgPos_Y = (box.top() - pointY) / ((sHeight - h) / (fHeight - h));
                if(imgPos_Y > 0.0)
                    imgPos_Y = 0.0;
            }
            break;

        // Scroll backround with divided offset at reference point edge
        case BgSetup::BgLayer::P_MODE_SCROLL:

            switch(layer.setup.reference_point_y)
            {
            case BgSetup::BgLayer::R_TOP:
                refPointY = box.top() - pointY + offsetYpre;
                break;
            case BgSetup::BgLayer::R_BOTTOM:
                refPointY = (box.bottom() - h) - pointY + offsetYpre;
                if(layer.setup.repeat_y)
                    offsetYpost = (h - fHeight) - (h - fmod(h, fHeight));
                else
                    offsetYpost = (h - fHeight);
                break;
            }

            if(layer.setup.repeat_y)
            {
                //If referrence point is positive (for example, autoscrolling have moved up), offset it
                if(refPointY > 0.0)
                    refPointY = -( (fHeight * layer.setup.parallax_coefficient_y)
                                    - std::fmod(refPointY, fHeight * layer.setup.parallax_coefficient_y) );
                imgPos_Y = std::fmod(refPointY / layer.setup.parallax_coefficient_y, fHeight) + offsetYpost;
            }
            else
            {
                imgPos_Y = (refPointY / layer.setup.parallax_coefficient_y) + offsetYpost;
            }
            break;

        // Fixed position
        case BgSetup::BgLayer::P_MODE_FIXED:
        bgSetupFixedH:
            switch(layer.setup.reference_point_y)
            {
            case BgSetup::BgLayer::R_TOP:
                imgPos_Y = offsetYpre;
                break;
            case BgSetup::BgLayer::R_BOTTOM:
                imgPos_Y = offsetYpre + (h - fHeight);
                break;
            }
            if(layer.setup.repeat_y && (imgPos_Y > 0.0))
                imgPos_Y = -( (fHeight) - std::fmod(imgPos_Y, fHeight));
            break;
        }



        AniPos ani_x(0, 1);
        if(layer.setup.animated) //Get current animated frame
            ani_x = ConfigManager::Animator_BG[static_cast<int>(layer.tID.animatorId)].image();

        double lenght_h = imgPos_X;
        double lenght_v = imgPos_Y;

        int horizontalRepeats = 1;
        int verticalRepeats = 1;

        if(layer.setup.repeat_x)
        {
            horizontalRepeats = 1;
            while((lenght_h <= w) || (lenght_h <= fWidth))
            {
                horizontalRepeats++;
                lenght_h += fWidth;
            }
        }

        if(layer.setup.repeat_y)
        {
            verticalRepeats = 1;
            while(lenght_v <= h  || (lenght_v <= fHeight))
            {
                verticalRepeats++;
                lenght_v += fHeight;
            }
        }

        double draw_x = imgPos_X;
        PGE_RectF m_backgrndG;

        GlRenderer::BindTexture(&layer.texture);
        GlRenderer::setTextureColor(1.0f, 1.0f, 1.0f, static_cast<float>(layer.setup.opacity));

        while(verticalRepeats > 0)
        {
            draw_x = imgPos_X;
            lenght_h = -fWidth;
            double d_left   = layer.setup.flip_h ? 1.0 : 0.0;
            double d_right  = layer.setup.flip_h ? 0.0 : 1.0;
            double d_top    = layer.setup.flip_v ? ani_x.second : ani_x.first;
            double d_bottom = layer.setup.flip_v ? ani_x.first: ani_x.second;

            double r_bottom = imgPos_Y + static_cast<double>(layer.texture.frame_h) + layer.setup.padding_y_top;
            if((imgPos_Y <= h) && (r_bottom >= 0.0))//Draw row if it is visible on screen
            {
                int hRepeats = horizontalRepeats;
                while(hRepeats > 0)
                {
                    double r_right = draw_x + static_cast<double>(layer.texture.frame_w) + layer.setup.padding_x_left;
                    if((draw_x <= w) && (r_right >= 0.0))//Draw cell if it is visible on screen
                    {
                        m_backgrndG.setRect(draw_x + layer.setup.padding_x_left,
                                            imgPos_Y + layer.setup.padding_y_top,
                                            layer.texture.frame_w,
                                            layer.texture.frame_h);
                        GlRenderer::renderTextureCur(static_cast<float>(m_backgrndG.left()),
                                                     static_cast<float>(m_backgrndG.top()),
                                                     static_cast<float>(m_backgrndG.width()),
                                                     static_cast<float>(m_backgrndG.height()),
                                                     static_cast<float>(d_top),
                                                     static_cast<float>(d_bottom),
                                                     static_cast<float>(d_left),
                                                     static_cast<float>(d_right));
                    }
                    hRepeats--;
                    lenght_h    += fWidth;
                    draw_x      += fWidth;
                }
            }
            verticalRepeats--;
            if(verticalRepeats > 0)
                imgPos_Y += fHeight;// * ((layer.setup.reference_point_y == BgSetup::BgLayer::R_BOTTOM) ? -1 : 1);
        }

        GlRenderer::UnBindTexture();
    }
}