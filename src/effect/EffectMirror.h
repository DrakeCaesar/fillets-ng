#ifndef HEADER_EFFECTMIRROR_H
#define HEADER_EFFECTMIRROR_H

#include "ViewEffect.h"

/**
 * Mirror reflect left side.
 */
class EffectMirror : public ViewEffect {
private:
    static const int MIRROR_BORDER = 3;

public:
    static const char *NAME;

    virtual const char *getName() const { return NAME; }

    virtual void blit(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Surface *surface,
                      int x, int y);

    static void cleanup();
};

#endif
