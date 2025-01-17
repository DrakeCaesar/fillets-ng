/*
 * Copyright (C) 2004 Ivo Danihelka (ivo@danihelka.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "VideoAgent.h"

#include "Log.h"
#include "Path.h"
#include "ImgException.h"
#include "SDLException.h"
#include "LogicException.h"
#include "AgentPack.h"
#include "SimpleMsg.h"
#include "StringMsg.h"
#include "UnknownMsgException.h"
#include "OptionAgent.h"
#include "SysVideo.h"

#include "SDL2/SDL_image.h"
#include <stdlib.h> // atexit()
#include <iostream>

//-----------------------------------------------------------------
/**
 * Init SDL and grafic window.
 * Register watcher for "fullscren" and "screen_*" options.
 * @throws SDLException if there is no usuable video mode
 */
void VideoAgent::own_init() {
    m_screen = NULL;
    m_fullscreen = false;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw SDLException(ExInfo("Init"));
    }
    atexit(SDL_Quit);

    setIcon(Path::dataReadPath("images/icon.png"));

    registerWatcher("fullscreen");
    initVideoMode();
}
//-----------------------------------------------------------------
/**
 * Draw all drawer from list.
 * First will be drawed first.
 */
void VideoAgent::own_update() {
    drawOn(m_screen, s_renderer);
    SDL_UpdateTexture(m_texture, NULL, m_screen->pixels, m_screen->pitch);
    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, m_texture, NULL, NULL);
    SDL_RenderPresent(m_renderer);
}
//-----------------------------------------------------------------
/**
 * Shutdown SDL.
 */
void VideoAgent::own_shutdown() {
    SDL_Quit();
}

//-----------------------------------------------------------------
/**
 * Load and set icon.
 * @throws ImgException
 */
void VideoAgent::setIcon(const Path &file) {
    SDL_Surface *icon = IMG_Load(file.getNative().c_str());
    if (NULL == icon) {
        throw ImgException(ExInfo("Load")
                                   .addInfo("file", file.getNative()));
    }
    SDL_SetWindowIcon(m_window, icon);
    SDL_FreeSurface(icon);
}

//-----------------------------------------------------------------
/**
 * Init video mode along options.
 * Change window only when necessary.
 *
 * @throws SDLException when video mode cannot be made,
 * the old video mode remain usable
 */
void VideoAgent::initVideoMode() {
    OptionAgent *options = OptionAgent::agent();
    int screen_width = options->getAsInt("screen_width", 640);
    int screen_height = options->getAsInt("screen_height", 480);

    SDL_SetWindowTitle(m_window, options->getParam("caption", "A game").c_str());

    if (NULL == m_screen || m_screen->w != screen_width || m_screen->h != screen_height) {
        changeVideoMode(screen_width, screen_height);
    }
}

V2 VideoAgent::scaleMouseLoc(const V2 &v) {
    return V2((v.getX() - m_offsetX) / m_sx, (v.getY() - m_offsetY) / m_sy);
}

//-----------------------------------------------------------------
/**
 * Init new video mode.
 * NOTE: m_screen pointer will change
 */
void VideoAgent::changeVideoMode(int screen_width, int screen_height) {
    OptionAgent *options = OptionAgent::agent();
    int screen_bpp = options->getAsInt("screen_bpp", 32);
    m_fullscreen = options->getAsBool("fullscreen", false);

    if (0 == m_window) {

        m_window = SDL_CreateWindow("Fish Fillets",
                                    SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED,
                                    screen_width, screen_height,
                                    m_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

        m_renderer = SDL_CreateRenderer(m_window, -1, 0);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        SDL_RenderSetLogicalSize(m_renderer, screen_width, screen_height);
    } else {
        SDL_RenderSetLogicalSize(m_renderer, screen_width, screen_height);
        SDL_SetWindowSize(m_window, screen_width, screen_height);
        SDL_FreeSurface(m_screen);
        SDL_DestroyTexture(m_texture);
    }

    SDL_GetWindowSize(m_window, &m_ww, &m_wh);
    SDL_RenderGetScale(m_renderer, &m_sx, &m_sy);
    SDL_RenderGetLogicalSize(m_renderer, &m_lw, &m_lh);

    m_offsetX = ((m_ww - ((float) m_lw * m_sx)) / 2);
    m_offsetY = ((m_wh - ((float) m_lh * m_sy)) / 2);

    SDL_Surface *newScreen = SDL_CreateRGBSurface(0, screen_width, screen_height, screen_bpp,
                                                  0x00FF0000,
                                                  0x0000FF00,
                                                  0x000000FF,
                                                  0xFF000000);

    m_texture = SDL_CreateTexture(m_renderer,
                                  SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  screen_width, screen_height);

    if (newScreen) {
        m_screen = newScreen;
        std::cout << "screen created" << std::endl;
        SDL_DestroyRenderer(s_renderer);
        s_renderer = SDL_CreateSoftwareRenderer(m_screen);
//        s_renderer = SDL_CreateRenderer(m_window, -1, 0);

        // NOTE: must be two times to change MouseState
        SDL_WarpMouseInWindow(m_window, screen_width / 2, screen_height / 2);
        SDL_WarpMouseInWindow(m_window, screen_width / 2, screen_height / 2);
    } else {
        throw SDLException(ExInfo("SetVideoMode")
                                   .addInfo("width", screen_width)
                                   .addInfo("height", screen_height)
                                   .addInfo("bpp", screen_bpp));
    }
}
//-----------------------------------------------------------------
/**
 * Obtain video information about best video mode.
 * @return best video flags
 */
int VideoAgent::getVideoFlags() {
    int videoFlags = 0;
    videoFlags |= SDL_SWSURFACE;

    return videoFlags;
}
//-----------------------------------------------------------------
/**
 *  Toggle fullscreen.
 */
void VideoAgent::toggleFullScreen() {
    m_fullscreen = !m_fullscreen;
    int success = SDL_SetWindowFullscreen(m_window, m_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if (!success) {
        // NOTE: some platforms need reinit video
        changeVideoMode(m_screen->w, m_screen->h);
    }
}
//-----------------------------------------------------------------
/**
 * Handle incoming message.
 * Messages:
 * - fullscreen ... toggle fullscreen
 *
 * @throws UnknownMsgException
 */
void VideoAgent::receiveSimple(const SimpleMsg *msg) {
    if (msg->equalsName("fullscreen")) {
        OptionAgent *options = OptionAgent::agent();
        bool toggle = !(options->getAsBool("fullscreen"));
        options->setPersistent("fullscreen", toggle);
    } else {
        throw UnknownMsgException(msg);
    }
}
//-----------------------------------------------------------------
/**
 * Handle incoming message.
 * Messages:
 * - param_changed(fullscreen) ... handle fullscreen
 *
 * @throws UnknownMsgException
 */
void VideoAgent::receiveString(const StringMsg *msg) {
    if (msg->equalsName("param_changed")) {
        std::string param = msg->getValue();
        if ("fullscreen" == param) {
            bool fs = OptionAgent::agent()->getAsBool("fullscreen");
            if (fs != m_fullscreen) {
                toggleFullScreen();
            }
        } else {
            throw UnknownMsgException(msg);
        }
    } else {
        throw UnknownMsgException(msg);
    }
}
