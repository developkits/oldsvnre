#include "game.h"
namespace projs
{
    vector<hitmsg> hits;
    vector<projent *> projs;

    VAR(IDF_PERSIST, shadowdebris, 0, 1, 1);
    VAR(IDF_PERSIST, shadowgibs, 0, 1, 1);
    VAR(IDF_PERSIST, shadoweject, 0, 1, 1);
    VAR(IDF_PERSIST, shadowents, 0, 1, 1);

    VAR(IDF_PERSIST, maxprojectiles, 1, 128, INT_MAX-1);

    VAR(IDF_PERSIST, ejectfade, 0, 3500, INT_MAX-1);
    VAR(IDF_PERSIST, ejectspin, 0, 1, 1);
    VAR(IDF_PERSIST, ejecthint, 0, 1, 1);

    FVAR(IDF_PERSIST, gibselasticity, -10000, 0.35f, 10000);
    FVAR(IDF_PERSIST, gibsrelativity, -10000, 0.95f, 10000);
    FVAR(IDF_PERSIST, gibswaterfric, 0, 2, 10000);
    FVAR(IDF_PERSIST, gibsweight, -10000, 150, 10000);

    FVAR(IDF_PERSIST, debriselasticity, -10000, 0.6f, 10000);
    FVAR(IDF_PERSIST, debriswaterfric, 0, 1.7f, 10000);
    FVAR(IDF_PERSIST, debrisweight, -10000, 165, 10000);

    FVAR(IDF_PERSIST, ejectelasticity, -10000, 0.35f, 10000);
    FVAR(IDF_PERSIST, ejectrelativity, -10000, 1, 10000);
    FVAR(IDF_PERSIST, ejectwaterfric, 0, 1.75f, 10000);
    FVAR(IDF_PERSIST, ejectweight, -10000, 180, 10000);

    VAR(IDF_PERSIST, projtrails, 0, 1, 1);
    VAR(IDF_PERSIST, projtraildelay, 1, 15, INT_MAX-1);
    VAR(IDF_PERSIST, projtraillength, 1, 350, INT_MAX-1);
    VAR(IDF_PERSIST, projfirehint, 0, 1, 1);
    VAR(IDF_PERSIST, projteamhint, 0, 0, 1);
    #define teamhint(a,b) (projteamhint ? (a)->colour() : b)

    VAR(IDF_PERSIST, muzzleflash, 0, 3, 3); // 0 = off, 1 = only other players, 2 = only thirdperson, 3 = all
    VAR(IDF_PERSIST, muzzleflare, 0, 2, 3); // 0 = off, 1 = only other players, 2 = only thirdperson, 3 = all
    FVAR(IDF_PERSIST, muzzleblend, 0, 1, 1);

    #define muzzlechk(a,b) (a == 3 || (a == 2 && game::thirdpersonview(true)) || (a == 1 && b != game::focus))
    #define notrayspam(a,b,c) (WEAP2(a, rays, b) <= c || !rnd(max(c, 2)))

    int calcdamage(gameent *actor, gameent *target, int weap, int &flags, int radial, float size, float dist, float scale)
    {
        int nodamage = 0; flags &= ~HIT_SFLAGS;
        if(actor->aitype < AI_START)
        {
            if((actor == target && !selfdamage) || (m_trial(game::gamemode) && !trialdamage)) nodamage++;
            else if(m_play(game::gamemode) && m_team(game::gamemode, game::mutators) && actor->team == target->team && actor != target)
            {
                switch(teamdamage)
                {
                    case 2: default: break;
                    case 1: if(actor->aitype < 0 || target->aitype >= 0) break;
                    case 0: nodamage++; break;
                }
            }
            if(m_expert(game::gamemode, game::mutators) && !hithead(flags)) nodamage++;
        }

        if(nodamage || !hithurts(flags))
        {
            flags &= ~HIT_CLEAR;
            flags |= HIT_WAVE;
        }

        float skew = damagescale*clamp(scale, 0.f, 1.f);
        if(radial) skew *= clamp(1.f-dist/size, 1e-6f, 1.f);
        else if(WEAP2(weap, taper, flags&HIT_ALT) > 0) skew *= clamp(dist, 0.f, 1.f);
        if(!(flags&HIT_HEAD))
        {
            if(flags&HIT_WHIPLASH) skew *= WEAP2(weap, whipdmg, flags&HIT_ALT);
            else if(flags&HIT_TORSO) skew *= WEAP2(weap, torsodmg, flags&HIT_ALT);
            else if(flags&HIT_LEGS) skew *= WEAP2(weap, legsdmg, flags&HIT_ALT);
            else skew = 0;
        }

        return int(ceilf((flags&HIT_FLAK ? WEAP2(weap, flakdmg, flags&HIT_ALT) : WEAP2(weap, damage, flags&HIT_ALT))*skew));
    }

    void hitpush(gameent *d, projent &proj, int flags = 0, int radial = 0, float dist = 0, float scale = 1)
    {
        vec dir, middle = d->headpos(-d->height*0.5f);
        dir = vec(middle).sub(proj.o);
        float dmag = dir.magnitude();
        if(dmag > 1e-3f) dir.div(dmag);
        else dir = vec(0, 0, 1);
        if(proj.child || !weaptype[proj.weap].traced)
        {
            float speed = proj.vel.magnitude();
            if(speed > 1e-6f)
            {
                dir.add(vec(proj.vel).div(speed));
                dmag = dir.magnitude();
                if(dmag > 1e-3f) dir.div(dmag);
                else dir = vec(0, 0, 1);
            }
        }
        if(proj.owner && (proj.owner == game::player1 || proj.owner->ai))
        {
            int hflags = proj.flags|flags;
            float size = hflags&HIT_WAVE ? radial*WEAP(proj.weap, pusharea) : radial;
            int damage = calcdamage(proj.owner, d, proj.weap, hflags, radial, size, dist, scale);
            game::hiteffect(proj.weap, hflags, damage, d, proj.owner, dir, false);
        }
        hitmsg &h = hits.add();
        h.flags = flags;
        h.proj = 0;
        h.target = d->clientnum;
        h.dist = int(dist*DNF);
        h.dir = ivec(int(dir.x*DNF), int(dir.y*DNF), int(dir.z*DNF));
    }

    void projpush(projent *p)
    {
        if(p->projtype == PRJ_SHOT && p->owner)
        {
            if(game::player1 == p->owner || p->owner->ai) p->state = CS_DEAD;
            else
            {
                hitmsg &h = hits.add();
                h.flags = HIT_PROJ|HIT_TORSO;
                h.proj = p->id;
                h.target = p->owner->clientnum;
                h.dist = 0;
                h.dir = ivec(0, 0, 0);
            }
        }
    }

    bool hiteffect(projent &proj, physent *d, int flags, const vec &norm)
    {
        if(proj.projtype == PRJ_SHOT && physics::issolid(d, &proj))
        {
            proj.hit = d;
            proj.hitflags = flags;
            float expl = WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize);
            if(proj.local && expl <= 0)
            {
                if(d->type == ENT_PLAYER || d->type == ENT_AI)
                {
                    int flags = 0;
                    if(proj.hitflags&HITFLAG_LEGS) flags |= HIT_LEGS;
                    if(proj.hitflags&HITFLAG_TORSO) flags |= HIT_TORSO;
                    if(proj.hitflags&HITFLAG_HEAD) flags |= HIT_HEAD;
                    if(flags) hitpush((gameent *)d, proj, flags|HIT_PROJ, 0, proj.lifesize, proj.curscale);
                }
                else if(d->type == ENT_PROJ) projpush((projent *)d);
            }
            switch(proj.weap)
            {
                case WEAP_RIFLE:
                    part_splash(PART_SPARK, 25, 500, proj.o, 0x6611FF, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale*0.125f, 1, 1, 0, 24, 20);
                    part_create(PART_PLASMA, 500, proj.o, 0x6611FF, expl*0.5f, 1, 0, 0);
                    adddynlight(proj.o, expl*1.1f, vec(0.4f, 0.05f, 1.f), 250, 10);
                    break;
                default:
                    if(weaptype[proj.weap].melee)
                    {
                        part_flare(proj.o, proj.from, 500, proj.weap == WEAP_SWORD ? PART_LIGHTNING_FLARE : PART_MUZZLE_FLARE, proj.weap == WEAP_SWORD ? 0x1111CC : 0xAA8811, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale, 0.75f);
                        part_create(PART_PLASMA, 500, proj.o, proj.weap == WEAP_SWORD ? 0x1111CC : 0xAA8811, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale);
                    }
                    break;
            }
            return (proj.projcollide&COLLIDE_CONT) ? false : true;
        }
        return false;
    }

    bool radialeffect(gameent *d, projent &proj, bool explode, int radius)
    {
        bool push = WEAP(proj.weap, pusharea) > 1, radiated = false;
        float maxdist = push ? radius*WEAP(proj.weap, pusharea) : radius;
        #define radialpush(xx,yx,yy,yz1,yz2,zz) \
            if(!proj.o.reject(xx, maxdist+max(yx, yy))) \
            { \
                vec bottom(xx), top(xx); bottom.z -= yz1; top.z += yz2; \
                zz = closestpointcylinder(proj.o, bottom, top, max(yx, yy)).dist(proj.o); \
            }
        if(d->type == ENT_PLAYER || d->type == ENT_AI)
        {
            if(!isaitype(d->aitype) || aistyle[d->aitype].hitbox)
            {
                float rdist[3] = { -1, -1, -1 };
                radialpush(d->legs, d->lrad.x, d->lrad.y, d->lrad.z, d->lrad.z, rdist[0]);
                radialpush(d->torso, d->trad.x, d->trad.y, d->trad.z, d->trad.z, rdist[1]);
                radialpush(d->head, d->hrad.x, d->hrad.y, d->hrad.z, d->hrad.z, rdist[2]);
                int closest = -1;
                loopi(3) if(rdist[i] >= 0 && (closest < 0 || rdist[i] <= rdist[closest])) closest = i;
                loopi(3) if(rdist[i] >= 0)
                {
                    int flag = 0;
                    switch(i)
                    {
                        case 2: flag = closest != i || rdist[i] > WEAP2(proj.weap, headmin, proj.flags&HIT_ALT) ? HIT_WHIPLASH : HIT_HEAD; break;
                        case 1: flag = HIT_TORSO; break;
                        case 0: default: flag = HIT_LEGS; break;
                    }
                    if(rdist[i] <= radius)
                    {
                        hitpush(d, proj, flag|(explode ? HIT_EXPLODE : HIT_BURN), radius, rdist[i], proj.curscale);
                        radiated = true;
                    }
                    else if(WEAP(proj.weap, pusharea) > 1 && rdist[i] <= maxdist)
                    {
                        hitpush(d, proj, flag|HIT_WAVE, radius, rdist[i], proj.curscale);
                        radiated = true;
                    }
                }
            }
            else
            {
                float dist = -1;
                radialpush(d->o, d->xradius, d->yradius, d->height, d->aboveeye, dist);
                if(dist >= 0)
                {
                    if(dist <= radius)
                    {
                        hitpush(d, proj, (m_expert(game::gamemode, game::mutators) ? HIT_WHIPLASH : HIT_TORSO)|(explode ? HIT_EXPLODE : HIT_BURN), radius, dist, proj.curscale);
                        radiated = true;
                    }
                    else if(WEAP(proj.weap, pusharea) > 1 && dist <= maxdist)
                    {
                        hitpush(d, proj, (m_expert(game::gamemode, game::mutators) ? HIT_WHIPLASH : HIT_TORSO)|HIT_WAVE, radius, dist, proj.curscale);
                        radiated = true;
                    }
                }
            }
        }
        else if(d->type == ENT_PROJ && explode)
        {
            float dist = -1;
            radialpush(d->o, d->xradius, d->yradius, d->height, d->aboveeye, dist);
            if(dist >= 0 && dist <= radius) projpush((projent *)d);
        }
        return radiated;
    }

    void remove(gameent *owner)
    {
        loopv(projs) if(projs[i]->owner == owner)
        {
            if(projs[i]->projtype == PRJ_SHOT)
            {
                delete projs[i];
                projs.removeunordered(i--);
            }
            else projs[i]->owner = NULL;
        }
    }

    void destruct(gameent *d, int id)
    {
        loopv(projs) if(projs[i]->owner == d && projs[i]->projtype == PRJ_SHOT && projs[i]->id == id)
        {
            projs[i]->state = CS_DEAD;
            break;
        }
    }

    void reset()
    {
        projs.deletecontents();
        projs.shrink(0);
    };

    void preload()
    {
        loopi(WEAP_MAX)
        {
            if(*weaptype[i].proj) loadmodel(weaptype[i].proj, -1, true);
            if(*weaptype[i].eprj) loadmodel(weaptype[i].eprj, -1, true);
        }
        const char *mdls[] = { "projs/gibs/gib01", "projs/gibs/gib02", "projs/gibs/gib03", "projs/debris/debris01", "projs/debris/debris02", "projs/debris/debris03", "projs/debris/debris04", "" };
        for(int i = 0; *mdls[i]; i++) loadmodel(mdls[i], -1, true);
    }

    void reflect(projent &proj, vec &pos)
    {
        bool speed = proj.vel.magnitude() > 0.01f;
        float elasticity = speed ? proj.elasticity : 1.f, reflectivity = speed ? proj.reflectivity : 0.f;
        if(elasticity > 0.f)
        {
            vec dir[2]; dir[0] = dir[1] = vec(proj.vel).normalize();
            float mag = proj.vel.magnitude()*elasticity; // conservation of energy
            dir[1].reflect(pos);
            if(!proj.lastbounce && reflectivity > 0.f)
            { // if projectile returns at 180 degrees [+/-]reflectivity, skew the reflection
                float aim[2][2] = { { 0.f, 0.f }, { 0.f, 0.f } };
                loopi(2) vectoyawpitch(dir[i], aim[0][i], aim[1][i]);
                loopi(2)
                {
                    float rmax = 180.f+reflectivity, rmin = 180.f-reflectivity,
                        off = aim[i][1]-aim[i][0];
                    if(fabs(off) <= rmax && fabs(off) >= rmin)
                    {
                        if(off > 0.f ? off > 180.f : off < -180.f)
                            aim[i][1] += rmax-off;
                        else aim[i][1] -= off-rmin;
                    }
                    while(aim[i][1] < 0.f) aim[i][1] += 360.f;
                    while(aim[i][1] >= 360.f) aim[i][1] -= 360.f;
                }
                vecfromyawpitch(aim[0][1], aim[1][1], 1, 0, dir[1]);
            }
            float minspeed = proj.minspeed;
            if(itemrepulsion > 0 && proj.projtype == PRJ_ENT && entities::ents.inrange(proj.id) && enttype[entities::ents[proj.id]->type].usetype == EU_ITEM)
            {
                #define entbump(x) \
                { \
                    if(overlapsbox(proj.o, itemrepulsion, itemrepulsion, x, itemrepulsion, itemrepulsion)) \
                    { \
                        vec nrm = vec(proj.o).sub(x).normalize(); \
                        dir[1].add(nrm).normalize(); \
                        minspeed = max(minspeed, itemrepelspeed); \
                        break; \
                    } \
                }
                loopv(projs) if(projs[i]->projtype == PRJ_ENT && projs[i] != &proj && entities::ents.inrange(projs[i]->id) && enttype[entities::ents[projs[i]->id]->type].usetype == EU_ITEM)
                    entbump(projs[i]->o);
                if(!minspeed) loopi(entities::lastusetype[EU_ITEM]) if(enttype[entities::ents[i]->type].usetype == EU_ITEM && entities::ents[i]->spawned)
                    entbump(entities::ents[i]->o);
            }
            if(!dir[1].iszero()) proj.vel = vec(dir[1]).mul(max(mag, minspeed));
        }
        else proj.vel = vec(0, 0, 0);
    }

    void bounce(projent &proj, bool ricochet)
    {
        if((proj.movement >= 1 || (proj.projtype == PRJ_SHOT && !proj.child && weaptype[proj.weap].traced)) && (!proj.lastbounce || lastmillis-proj.lastbounce >= 250)) switch(proj.projtype)
        {
            case PRJ_SHOT:
            {
                switch(proj.weap)
                {
                    case WEAP_SWORD:
                    {
                        part_splash(PART_SPARK, 25, 500, proj.o, 0x1111CC, 0.25f, 1, 1, 0, 16, 10);
                        break;
                    }
                    case WEAP_SHOTGUN: case WEAP_SMG:
                    {
                        part_splash(PART_SPARK, 10, 500, proj.o, 0xFFAA22, 0.25f, 1, 1, 0, 16, 10);
                        if(notrayspam(proj.weap, proj.flags&HIT_ALT, 5)) adddecal(DECAL_BULLET, proj.o, proj.norm, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale);
                        break;
                    }
                    case WEAP_FLAMER:
                    {
                        if(notrayspam(proj.weap, proj.flags&HIT_ALT, 0))
                            adddecal(DECAL_SCORCH_SHORT, proj.o, proj.norm, WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize));
                        break;
                    }
                    default: break;
                }
                if(ricochet)
                {
                    int mag = int(proj.vel.magnitude()), vol = int(ceilf(clamp(mag*2, 10, 255)*proj.curscale*(proj.child ? 0.25f : 1.f)));
                    playsound(WEAPSND2(proj.weap, proj.flags&HIT_ALT, S_W_BOUNCE), proj.o, NULL, 0, vol);
                }
                break;
            }
            case PRJ_GIBS:
            {
                if(!kidmode && game::bloodscale > 0)
                {
                    adddecal(DECAL_BLOOD, proj.o, proj.norm, proj.radius*clamp(proj.vel.magnitude()/2, 1.f, 4.f), bvec(125, 255, 255));
                    int mag = int(proj.vel.magnitude()), vol = int(ceilf(clamp(mag*2, 10, 255)*proj.curscale));
                    playsound(S_SPLOSH+rnd(S_R_SPLOSH), proj.o, NULL, 0, vol);
                    break;
                } // otherwise fall through
            }
            case PRJ_DEBRIS:
            {
                int mag = int(proj.vel.magnitude()), vol = int(ceilf(clamp(mag*2, 10, 255)*proj.curscale));
                playsound(S_DEBRIS+rnd(S_R_DEBRIS), proj.o, NULL, 0, vol);
                break;
            }
            case PRJ_EJECT: case PRJ_AFFINITY:
            {
                int mag = int(proj.vel.magnitude()), vol = int(ceilf(clamp(mag*2, 10, 255)*proj.curscale));
                playsound(S_SHELL+rnd(S_R_SHELL), proj.o, NULL, 0, vol);
                break;
            }
            default: break;
        }
    }

    void updatebb(projent &proj, bool init = false)
    {
        float size = 1;
        switch(proj.projtype)
        {
            case PRJ_AFFINITY: break;
            case PRJ_GIBS: case PRJ_DEBRIS: case PRJ_EJECT: size = proj.lifesize;
            case PRJ_ENT:
                if(init) break;
                else if(proj.lifemillis && proj.fadetime)
                {
                    int interval = min(proj.lifemillis, proj.fadetime);
                    if(proj.lifetime < interval)
                    {
                        size *= float(proj.lifetime)/float(interval);
                        break;
                    }
                } // all falls through to ..
            default: return;
        }
        model *m = NULL;
        if(proj.mdl && *proj.mdl && ((m = loadmodel(proj.mdl)) != NULL))
        {
            vec center, radius;
            m->boundbox(0, center, radius);
            proj.xradius = proj.yradius = proj.radius = max(radius.x, radius.y)*size*proj.curscale;
            proj.height = proj.zradius = proj.aboveeye = radius.z*size*proj.curscale;
        }
        else switch(proj.projtype)
        {
            case PRJ_GIBS: case PRJ_DEBRIS: proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 0.5f*size*proj.curscale; break;
            case PRJ_EJECT: proj.height = proj.aboveeye = 0.25f*size*proj.curscale; proj.radius = proj.yradius = 0.5f*size*proj.curscale; proj.xradius = 0.125f*size*proj.curscale; break;
            case PRJ_ENT:
            {
                if(entities::ents.inrange(proj.id))
                    proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = enttype[entities::ents[proj.id]->type].radius*0.25f*size*proj.curscale;
                else proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = size*proj.curscale;
                break;
            }
        }
        if(init)
        {
            vec orig = proj.o;
            loopi(100)
            {
                if(i) proj.o.add(vec((rnd(21)-10)*i/10.f, (rnd(21)-10)*i/10.f, (rnd(21)-10)*i/10.f));
                if(collide(&proj) && !inside && (!hitplayer || !physics::issolid(hitplayer, &proj))) break;
                proj.o = orig;
            }
        }
    }

    void updatetargets(projent &proj, int style = 0)
    {
        if(proj.projtype == PRJ_SHOT && proj.weap != WEAP_MELEE && !proj.child && proj.owner && proj.owner->state == CS_ALIVE)
        {
            if(weaptype[proj.weap].traced)
            {
                proj.from = proj.owner->originpos();
                proj.to = proj.owner->muzzlepos(proj.weap, proj.flags&HIT_ALT);
                if(style != 2) proj.o = proj.from;
            }
            else
            {
                proj.from = proj.owner->muzzlepos(proj.weap, proj.flags&HIT_ALT);
                if(style == 1) proj.o = proj.from;
            }
        }
    }

    void init(projent &proj, bool waited)
    {
        if(waited && !proj.child && proj.owner && proj.owner->state == CS_ALIVE)
        {
            proj.inertia = vec(proj.owner->vel).add(proj.owner->falling);
            proj.yaw = proj.owner->yaw;
            proj.pitch = proj.owner->pitch;
        }
        switch(proj.projtype)
        {
            case PRJ_SHOT:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = WEAP2(proj.weap, radius, proj.flags&HIT_ALT);
                proj.elasticity = WEAP2(proj.weap, elasticity, proj.flags&HIT_ALT);
                proj.reflectivity = WEAP2(proj.weap, reflectivity, proj.flags&HIT_ALT);
                proj.relativity = WEAP2(proj.weap, relativity, proj.flags&HIT_ALT);
                proj.waterfric = WEAP2(proj.weap, waterfric, proj.flags&HIT_ALT);
                proj.weight = WEAP2(proj.weap, weight, proj.flags&HIT_ALT);
                proj.projcollide = WEAP2(proj.weap, collide, proj.flags&HIT_ALT);
                if(proj.child)
                {
                    proj.projcollide &= ~(IMPACT_GEOM|BOUNCE_PLAYER|IMPACT_SHOTS|COLLIDE_SHOTS|COLLIDE_CONT);
                    proj.projcollide |= BOUNCE_GEOM|IMPACT_PLAYER;
                }
                proj.extinguish = WEAP2(proj.weap, extinguish, proj.flags&HIT_ALT)|4;
                proj.lifesize = 1;
                proj.mdl = weaptype[proj.weap].proj;
                proj.escaped = !proj.owner || weaptype[proj.weap].traced;
                updatetargets(proj, waited ? 1 : 0);
                break;
            }
            case PRJ_GIBS:
            {
                if(!kidmode)
                {
                    proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 0.5f;
                    proj.lifesize = 1.5f-(rnd(100)/100.f);
                    if(proj.owner)
                    {
                        if(proj.owner->state == CS_DEAD || proj.owner->state == CS_WAITING)
                            proj.o = ragdollcenter(proj.owner);
                        else
                        {
                            proj.lifemillis = proj.lifetime = 1;
                            proj.lifespan = 1.f;
                            proj.state = CS_DEAD;
                            proj.escaped = true;
                            return;
                        }
                    }
                    switch(rnd(3))
                    {
                        case 2: proj.mdl = "projs/gibs/gib03"; break;
                        case 1: proj.mdl = "projs/gibs/gib02"; break;
                        case 0: default: proj.mdl = "projs/gibs/gib01"; break;
                    }
                    proj.reflectivity = 0.f;
                    proj.elasticity = gibselasticity;
                    proj.relativity = gibsrelativity;
                    proj.waterfric = gibswaterfric;
                    proj.weight = gibsweight*proj.lifesize;
                    proj.vel.add(vec(rnd(21)-10, rnd(21)-10, rnd(21)-10));
                    proj.projcollide = BOUNCE_GEOM|BOUNCE_PLAYER;
                    proj.escaped = !proj.owner || proj.owner->state != CS_ALIVE;
                    proj.fadetime = rnd(250)+250;
                    proj.extinguish = 6;
                    break;
                } // otherwise fall through
            }
            case PRJ_DEBRIS:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 0.5f;
                proj.lifesize = 1.5f-(rnd(100)/100.f);
                switch(rnd(4))
                {
                    case 3: proj.mdl = "projs/debris/debris04"; break;
                    case 2: proj.mdl = "projs/debris/debris03"; break;
                    case 1: proj.mdl = "projs/debris/debris02"; break;
                    case 0: default: proj.mdl = "projs/debris/debris01"; break;
                }
                proj.relativity = proj.reflectivity = 0.f;
                proj.elasticity = debriselasticity;
                proj.waterfric = debriswaterfric;
                proj.weight = debrisweight*proj.lifesize;
                proj.vel.add(vec(rnd(101)-50, rnd(101)-50, rnd(151)-50)).mul(2);
                proj.projcollide = BOUNCE_GEOM|BOUNCE_PLAYER|COLLIDE_OWNER;
                proj.escaped = !proj.owner || proj.owner->state != CS_ALIVE;
                proj.fadetime = rnd(250)+250;
                proj.extinguish = 1;
                break;
            }
            case PRJ_EJECT:
            {
                proj.height = proj.aboveeye = 0.5f; proj.radius = proj.yradius = 1; proj.xradius = 0.25f;
                if(!isweap(proj.weap) && proj.owner) proj.weap = proj.owner->weapselect;
                if(isweap(proj.weap))
                {
                    if(proj.owner) proj.o = proj.from = proj.owner->ejectpos(proj.weap);
                    proj.mdl = weaptype[proj.weap].eject && *weaptype[proj.weap].eprj ? weaptype[proj.weap].eprj : "projs/catridge";
                    proj.lifesize = weaptype[proj.weap].esize;
                    proj.light.material[0] = bvec(weaptype[proj.weap].colour);
                }
                else
                {
                    proj.mdl = "projs/catridge";
                    proj.lifesize = 1;
                }
                proj.reflectivity = 0.f;
                proj.elasticity = ejectelasticity;
                proj.relativity = ejectrelativity;
                proj.waterfric = ejectwaterfric;
                proj.weight = (ejectweight+(proj.speed*2))*proj.lifesize; // so they fall better in relation to their speed
                proj.projcollide = BOUNCE_GEOM;
                proj.escaped = true;
                proj.fadetime = rnd(300)+300;
                proj.extinguish = 6;
                if(proj.owner == game::focus && !game::thirdpersonview())
                    proj.o = proj.from.add(vec(proj.from).sub(camera1->o).normalize().mul(4));
                vecfromyawpitch(proj.yaw+40+rnd(41), proj.pitch+50-proj.speed+rnd(41), 1, 0, proj.to);
                proj.to.add(proj.from);
                break;
            }
            case PRJ_ENT:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 4;
                proj.mdl = entities::entmdlname(entities::ents[proj.id]->type, entities::ents[proj.id]->attrs);
                proj.lifesize = 1.f;
                proj.reflectivity = 0.f;
                proj.elasticity = itemelasticity;
                proj.relativity = itemrelativity;
                proj.waterfric = itemwaterfric;
                proj.weight = itemweight;
                proj.projcollide = itemcollide;
                proj.minspeed = itemminspeed;
                proj.escaped = true;
                float mag = proj.inertia.magnitude();
                if(mag <= 50)
                {
                    if(mag <= 0) vecfromyawpitch(proj.yaw, proj.pitch, 1, 0, proj.inertia);
                    proj.inertia.normalize().mul(50);
                }
                proj.to.add(proj.inertia);
                proj.to.z += 4;
                if(proj.flags) proj.inertia.div(proj.flags+1);
                proj.fadetime = 500;
                proj.extinguish = itemextinguish;
                break;
            }
            case PRJ_AFFINITY:
            {
                proj.height = proj.aboveeye = proj.radius = proj.xradius = proj.yradius = 4;
                vec dir = vec(proj.to).sub(proj.from).normalize();
                vectoyawpitch(dir, proj.yaw, proj.pitch);
                proj.lifesize = 1.f;
                proj.reflectivity = 0.f;
                proj.escaped = true;
                proj.fadetime = 500;
                switch(game::gamemode)
                {
                    case G_BOMBER:
                        proj.mdl = "ball";
                        proj.projcollide = bombercollide;
                        proj.extinguish = bomberextinguish;
                        proj.elasticity = bomberelasticity;
                        proj.weight = bomberweight;
                        proj.relativity = bomberrelativity;
                        proj.waterfric = bomberwaterfric;
                        proj.minspeed = bomberminspeed;
                        break;
                    case G_CAPTURE: default:
                        proj.mdl = "flag";
                        proj.projcollide = capturecollide;
                        proj.extinguish = captureextinguish;
                        proj.elasticity = captureelasticity;
                        proj.weight = captureweight;
                        proj.relativity = capturerelativity;
                        proj.waterfric = capturewaterfric;
                        proj.minspeed = captureminspeed;
                        break;
                }
                break;
            }
            default: break;
        }
        if(proj.projtype != PRJ_SHOT) updatebb(proj, true);
        if(!proj.child && (proj.projtype != PRJ_SHOT || !weaptype[proj.weap].traced))
        {
            vec dir = vec(proj.to).sub(proj.o), orig = proj.o;
            float maxdist = dir.magnitude();
            if(maxdist > 1e-3f)
            {
                dir.mul(1/maxdist);
                if(proj.projtype != PRJ_EJECT) vectoyawpitch(dir, proj.yaw, proj.pitch);
            }
            else if(proj.owner)
            {
                if(proj.projtype != PRJ_EJECT)
                {
                    proj.yaw = proj.owner->yaw;
                    proj.pitch = proj.owner->pitch;
                }
                vecfromyawpitch(proj.yaw, proj.pitch, 1, 0, dir);
            }
            vec rel = vec(proj.vel).add(dir);
            if(proj.relativity > 0)
            {
                if(proj.owner) loopi(3) if(proj.inertia[i]*rel[i] < 0) proj.inertia[i] = 0;
                rel.add(proj.inertia.mul(proj.relativity));
            }
            proj.vel = vec(rel).add(vec(dir).mul(physics::movevelocity(&proj)));
        }
        proj.spawntime = lastmillis;
        proj.hit = NULL;
        proj.hitflags = HITFLAG_NONE;
        proj.movement = 1;
        if(proj.projtype == PRJ_SHOT && !proj.child && proj.owner && !weaptype[proj.weap].traced)
        {
            vec eyedir = vec(proj.o).sub(proj.owner->o);
            float eyedist = eyedir.magnitude();
            if(eyedist >= 1e-3f)
            {
                eyedir.div(eyedist);
                float blocked = pltracecollide(&proj, proj.owner->o, eyedir, eyedist);
                if(blocked >= 0) proj.o = vec(eyedir).mul(blocked-0.1f).add(proj.owner->o);
            }
        }
        proj.resetinterp();
    }

    projent *create(const vec &from, const vec &to, bool local, gameent *d, int type, int lifetime, int lifemillis, int waittime, int speed, int id, int weap, int flags, float scale, bool child, projent *parent)
    {
        projent &proj = *new projent;
        proj.o = proj.from = from;
        proj.to = to;
        proj.local = local;
        proj.projtype = type;
        proj.addtime = lastmillis;
        proj.lifetime = lifetime;
        proj.lifemillis = lifemillis ? lifemillis : proj.lifetime;
        proj.waittime = waittime;
        proj.bspeed = proj.speed = speed;
        proj.id = id;
        proj.flags = flags;
        proj.curscale = scale;
        proj.movement = 0;
        if(proj.projtype == PRJ_AFFINITY)
        {
            proj.vel = proj.inertia = proj.to;
            proj.to.add(proj.from);
            if(weap >= 0) proj.target = game::getclient(weap);
        }
        else
        {
            proj.weap = weap;
            if(child)
            {
                proj.child = true;
                proj.owner = d;
                proj.vel = vec(proj.to).sub(proj.from);
                if(parent) proj.target = parent->target;
            }
            else if(d)
            {
                proj.owner = d;
                proj.yaw = d->yaw;
                proj.pitch = d->pitch;
                proj.inertia = vec(d->vel).add(d->falling);
                if(proj.projtype == PRJ_SHOT && isweap(proj.weap) && issound(d->pschan))
                    playsound(WEAPSND2(proj.weap, proj.flags&HIT_ALT, S_W_TRANSIT), proj.o, &proj, SND_LOOP, int(ceilf(sounds[d->pschan].vol*(proj.child ? 0.25f : 1.f))), -1, -1, &proj.schan, 0, &d->pschan);
            }
        }
        if(!proj.waittime) init(proj, false);
        projs.add(&proj);
        return &proj;
    }

    void drop(gameent *d, int g, int n, int v, bool local, int c)
    {
        if(g >= WEAP_OFFSET && isweap(g))
        {
            if(v >= 0)
            {
                if(entities::ents.inrange(n))
                    create(d->muzzlepos(), d->muzzlepos(), local, d, PRJ_ENT, w_spawn(g), w_spawn(g), 1, 1, n, v, c);
                d->ammo[g] = -1;
                d->setweapstate(g, WEAP_S_SWITCH, WEAPSWITCHDELAY, lastmillis);
            }
            else if(g == WEAP_GRENADE)
                create(d->muzzlepos(), d->muzzlepos(), local, d, PRJ_SHOT, 1, WEAP2(g, time, false), 1, 1, 1, g);
            d->entid[g] = -1;
        }
    }

    void shootv(int weap, int flags, int offset, float scale, vec &from, vector<shotmsg> &shots, gameent *d, bool local)
    {
        int delay = WEAP2(weap, pdelay, flags&HIT_ALT), adelay = WEAP2(weap, adelay, flags&HIT_ALT),
            life = WEAP2(weap, time, flags&HIT_ALT), speed = WEAP2(weap, speed, flags&HIT_ALT);

        if(WEAP2(weap, power, flags&HIT_ALT)) switch(WEAP2(weap, cooked, flags&HIT_ALT))
        {
            case 1: break;
            case 2: case 3: life = int(ceilf(life*(1.f-scale))); if(!(WEAP2(weap, cooked, flags&HIT_ALT)%2)) scale = 1; break; // shorter
            case 4: case 5: life = int(ceilf(life*scale)); if(!(WEAP2(weap, cooked, flags&HIT_ALT)%2)) scale = 1; break; // longer
            default: scale = 1; break;
        }

        if(weaptype[weap].sound >= 0)
        {
            int slot = WEAPSNDF(weap, flags&HIT_ALT), vol = int(ceilf(255*scale));
            if(slot >= 0 && vol > 0)
            {
                if(weap == WEAP_FLAMER && !(flags&HIT_ALT))
                {
                    int ends = lastmillis+WEAP2(weap, adelay, flags&HIT_ALT)+PHYSMILLIS;
                    if(issound(d->wschan)) sounds[d->wschan].ends = ends;
                    else playsound(slot+rnd(S_W_SHOOT), d->o, d, (d == game::focus ? SND_FORCED : 0)|SND_LOOP, vol, -1, -1, &d->wschan, ends);
                }
                else if(!WEAP2(weap, time, flags&HIT_ALT) || life)
                {
                    if(issound(d->wschan) && sounds[d->wschan].slotnum >= WEAPSNDF(weap, false) && sounds[d->wschan].slotnum < WEAPSNDF(weap, true)+S_W_SHOOT)
                    {
                        sounds[d->wschan].hook = NULL;
                        d->wschan = -1;
                    }
                    playsound(slot+rnd(S_W_SHOOT), d->o, d, d == game::focus ? SND_FORCED : 0, vol, -1, -1, &d->wschan);
                }
            }
        }
        float muz = muzzleblend; if(d == game::focus) muz *= 0.5f;
        const struct weapfxs
        {
            int smoke, parttype, colour, sparktime, sparknum, sparkrad;
            float partsize, flaresize, flarelen, sparksize;
        } weapfx[WEAP_MAX] = {
            { 0, 0, -1, 0, 0, 0, 0, 0, 0, 0 },
            { 200, PART_MUZZLE_FLASH, 0xFFCC22, 200, 5, 4, 1, 1, 2, 0.0125f },
            { 0, 0, -1, 0, 0, 0, 0, 0, 0, 0 },
            { 350, PART_MUZZLE_FLASH, 0xFFAA00, 500, 20, 8, 3, 3, 6, 0.025f },
            { 50, PART_MUZZLE_FLASH, 0xFF8800, 350, 5, 6, 1.5f, 2, 4, 0.0125f },
            { 150, PART_MUZZLE_FLASH, 0, 250, 5, 8, 1.5f, 0, 0, 0.025f },
            { 150, PART_PLASMA, 0x226688, 250, 10, 6, 1.5f, 0, 0, 0.0125f },
            { 150, PART_PLASMA, 0x6611FF, 250, 5, 6, 2, 3, 6, 0.0125f },
            { 0, 0, -1, 0, 0, 0, 0, 0, 0, 0 },
            { 150, PART_MUZZLE_FLASH, 0, 250, 10, 8, 3, 3, 6, 0.0125f },
        };
        if(weapfx[weap].colour >= 0 && WEAP2(weap, adelay, flags&HIT_ALT) >= 5)
        {
            bool isfire = weapfx[weap].colour > 0;
            int colour = isfire ? weapfx[weap].colour : pulsecols[0][rnd(PULSECOLOURS)];
            if(weapfx[weap].smoke) part_create(PART_SMOKE_LERP, weapfx[weap].smoke, from, 0xAAAAAA, 1, 0.25f, -20);
            if(weapfx[weap].sparktime && weapfx[weap].sparknum)
                part_splash(isfire ? PART_FIREBALL : PART_SPARK, weapfx[weap].sparknum, weapfx[weap].sparktime, from, colour, weapfx[weap].sparksize, muz, 1, 0, weapfx[weap].sparkrad, 10);
            if(muzzlechk(muzzleflash, d) && weapfx[weap].partsize > 0)
                part_create(weapfx[weap].parttype, WEAP2(weap, adelay, flags&HIT_ALT)/3, from, colour, weapfx[weap].partsize, muz, 0, 0, d);
            if(muzzlechk(muzzleflare, d) && weapfx[weap].flaresize > 0)
            {
                vec targ; findorientation(d->o, d->yaw, d->pitch, targ);
                targ.sub(from).normalize().mul(weapfx[weap].flarelen).add(from);
                part_flare(from, targ, WEAP2(weap, adelay, flags&HIT_ALT)/2, PART_MUZZLE_FLARE, colour, weapfx[weap].flaresize, muz, 0, 0, d);
            }
            int peak = WEAP2(weap, adelay, flags&HIT_ALT)/4, fade = min(peak/2, 75);
            adddynlight(from, 32, vec(colour>>16, (colour>>8)&0xFF, colour&0xFF).div(512.f), fade, peak - fade, DL_FLASH);
        }

        loopv(shots)
            create(from, shots[i].pos.tovec().div(DMF), local, d, PRJ_SHOT, max(life, 1), WEAP2(weap, time, flags&HIT_ALT), delay, speed, shots[i].id, weap, flags, scale);
        if(ejectfade && weaptype[weap].eject && *weaptype[weap].eprj) loopi(clamp(offset, 1, WEAP2(weap, sub, flags&HIT_ALT)))
            create(from, from, local, d, PRJ_EJECT, rnd(ejectfade)+ejectfade, 0, delay, rnd(weaptype[weap].espeed)+weaptype[weap].espeed, 0, weap, flags);

        if(d->aitype >= AI_BOT && d->skill <= 100 && (!WEAP2(weap, fullauto, flags&HIT_ALT) || adelay >= PHYSMILLIS))
            adelay += int(ceilf(adelay*(10.f/d->skill)));
        d->setweapstate(weap, flags&HIT_ALT ? WEAP_S_SECONDARY : WEAP_S_PRIMARY, adelay, lastmillis);
        d->ammo[weap] = max(d->ammo[weap]-offset, 0);
        d->weapshot[weap] = offset;
    }

    void iter(projent &proj)
    {
        proj.lifespan = clamp((proj.lifemillis-proj.lifetime)/float(max(proj.lifemillis, 1)), 0.f, 1.f);
        if(proj.target && proj.target->state != CS_ALIVE) proj.target = NULL;
        if(proj.projtype == PRJ_SHOT)
        {
            updatetargets(proj);
            if(WEAP2(proj.weap, taper, proj.flags&HIT_ALT) > 0)
            {
                if(WEAP2(proj.weap, taperspan, proj.flags&HIT_ALT) > 0)
                {
                    if(proj.lifespan > WEAP2(proj.weap, taperspan, proj.flags&HIT_ALT))
                    {
                        if(!proj.stuck) proj.lifesize = clamp((1.f+WEAP2(proj.weap, taperspan, proj.flags&HIT_ALT))-proj.lifespan, 1.f-WEAP2(proj.weap, taper, proj.flags&HIT_ALT), 1.f);
                    }
                    else proj.lifesize = proj.lifespan*(1.f/WEAP2(proj.weap, taperspan, proj.flags&HIT_ALT));
                }
                else if(!proj.stuck) proj.lifesize = clamp(1.f-proj.lifespan, 1.f-WEAP2(proj.weap, taper, proj.flags&HIT_ALT), 1.f);
            }
            else if(WEAP2(proj.weap, radial, proj.flags&HIT_ALT)) proj.lifesize = proj.lifespan;
            else proj.lifesize = 1;
        }
        updatebb(proj);
    }

    void effect(projent &proj)
    {
        switch(proj.projtype)
        {
            case PRJ_SHOT:
            {
                int vol = int(ceilf(255*proj.curscale*(proj.child ? 0.25f : 1.f)));
                if(WEAP2(proj.weap, power, proj.flags&HIT_ALT)) switch(WEAP2(proj.weap, cooked, proj.flags&HIT_ALT))
                {
                    case 4: case 5: vol = 10+int(245*(1.f-proj.lifespan)*proj.lifesize*proj.curscale*(proj.child ? 0.25f : 1.f)); break; // longer
                    case 1: case 2: case 3: default: vol = 10+int(245*proj.lifespan*proj.lifesize*proj.curscale*(proj.child ? 0.25f : 1.f)); break; // shorter
                }
                if(issound(proj.schan)) sounds[proj.schan].vol = vol;
                else playsound(WEAPSND2(proj.weap, proj.flags&HIT_ALT, S_W_TRANSIT), proj.o, &proj, SND_LOOP, vol, -1, -1, &proj.schan);
                switch(proj.weap)
                {
                    case WEAP_SWORD:
                    {
                        part_flare(proj.from, proj.to, 1, PART_LIGHTNING_FLARE, 0x1111CC, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale, 1);
                        if(lastmillis-proj.lasteffect >= 25)
                        {
                            part_flare(proj.from, proj.to, 150, PART_LIGHTNING_FLARE, 0x1111CC, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale, 0.75f);
                            proj.lasteffect = lastmillis - (lastmillis%25);
                        }
                        break;
                    }
                    case WEAP_PISTOL:
                    {
                        float size = clamp(WEAP2(proj.weap, partlen, proj.flags&HIT_ALT)*(1.f-proj.lifespan)*proj.curscale, proj.curscale, min(min(WEAP2(proj.weap, partlen, proj.flags&HIT_ALT), proj.movement), proj.o.dist(proj.from)));
                        if(size > 0)
                        {
                            proj.to = vec(proj.o).sub(vec(proj.vel).normalize().mul(size));
                            int col = ((int(220*max(1.f-proj.lifespan,0.3f))<<16))|((int(160*max(1.f-proj.lifespan,0.2f)))<<8);
                            part_flare(proj.to, proj.o, 1, PART_FLARE, teamhint(proj.owner, col), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale, 0.25f);
                            part_flare(proj.to, proj.o, 1, PART_FLARE_LERP, col, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale*0.5f);
                            part_create(PART_PLASMA, 1, proj.o, col, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale);
                            part_create(PART_HINT, 1, proj.o, teamhint(proj.owner, col), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*1.125f*proj.curscale);
                        }
                        break;
                    }
                    case WEAP_FLAMER:
                    {
                        float scale = lastmillis-proj.spawntime <= proj.lifemillis/10 ? (lastmillis-proj.spawntime)/float(proj.lifemillis/10) : 1,
                            size = WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*1.25f*proj.lifespan*proj.curscale*scale, blend = clamp(1.25f-proj.lifespan, 0.35f, 0.85f)*(0.6f+(rnd(40)/100.f))*(proj.owner == game::focus ? 0.5f : 0.75f);
                        if(projfirehint && notrayspam(proj.weap, proj.flags&HIT_ALT, 1))
                            part_create(PART_HINT_SOFT, 1, proj.o, teamhint(proj.owner, 0x120228), size*1.5f, blend);
                        if(projtrails && lastmillis-proj.lasteffect >= projtraildelay*2)
                        {
                            part_create(PART_FIREBALL_SOFT, max(int(projtraillength*0.5f*max(1.f-proj.lifespan, 0.1f)), 1), proj.o, pulsecols[0][rnd(PULSECOLOURS)], size, blend, -5);
                            proj.lasteffect = lastmillis - (lastmillis%(projtraildelay*2));
                        }
                        else part_create(PART_FIREBALL_SOFT, 1, proj.o, pulsecols[0][rnd(PULSECOLOURS)], size, blend);
                        break;
                    }
                    case WEAP_GRENADE:
                    {
                        int col = ((int(254*max(1.f-proj.lifespan,0.5f))<<16)+1)|((int(98*max(1.f-proj.lifespan,0.f))+1)<<8), interval = lastmillis%1000;
                        float fluc = 1.f+(interval ? (interval <= 500 ? interval/500.f : (1000-interval)/500.f) : 0.f);
                        part_create(PART_PLASMA_SOFT, 1, proj.o, col, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*fluc);
                        part_create(PART_HINT_SOFT, 1, proj.o, teamhint(proj.owner, col), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*1.125f*fluc, 0.5f);
                        if(projtrails && lastmillis-proj.lasteffect >= projtraildelay)
                        {
                            part_create(PART_SMOKE_LERP, projtraillength, proj.o, 0x888888, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT), 0.75f, -10);
                            proj.lasteffect = lastmillis - (lastmillis%projtraildelay);
                        }
                        break;
                    }
                    case WEAP_ROCKET:
                    {
                        int col = ((int(254*max(1.f-proj.lifespan,0.5f))<<16)+1)|((int(98*max(1.f-proj.lifespan,0.f))+1)<<8), interval = lastmillis%1000;
                        float fluc = 1.f+(interval ? (interval <= 500 ? interval/500.f : (1000-interval)/500.f) : 0.f);
                        part_create(PART_PLASMA_SOFT, 1, proj.o, col, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*fluc);
                        part_create(PART_HINT_SOFT, 1, proj.o, teamhint(proj.owner, col), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*1.125f*fluc, 0.5f);
                        if(projtrails && lastmillis-proj.lasteffect >= projtraildelay)
                        {
                            part_create(PART_FIREBALL_SOFT, max(projtraillength/2, 1), proj.o, pulsecols[0][rnd(PULSECOLOURS)], WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*0.5f, 0.5f, -5);
                            part_create(PART_SMOKE_LERP, projtraillength, proj.o, 0x222222, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT), 1.f, -10);
                            proj.lasteffect = lastmillis - (lastmillis%projtraildelay);
                        }
                        break;
                    }
                    case WEAP_SHOTGUN:
                    {
                        int col = ((int(224*max(1.f-proj.lifespan,0.3f))<<16)+1)|((int(144*max(1.f-proj.lifespan,0.15f))+1)<<8);
                        if(!proj.child && WEAP2(proj.weap, flakweap, proj.flags&HIT_ALT) >= 0)
                        {
                            part_create(PART_PLASMA, 1, proj.o, col, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*10.f*proj.curscale);
                            part_create(PART_HINT_SOFT, 1, proj.o, teamhint(proj.owner, col), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*11.25f*proj.curscale, 0.5f);
                        }
                        else
                        {
                            float size = clamp(WEAP2(proj.weap, partlen, proj.flags&HIT_ALT)*(1.f-proj.lifespan)*proj.curscale, proj.curscale, min(min(WEAP2(proj.weap, partlen, proj.flags&HIT_ALT), proj.movement), proj.o.dist(proj.from)));
                            if(size > 0)
                            {
                                proj.to = vec(proj.o).sub(vec(proj.vel).normalize().mul(size));
                                part_flare(proj.to, proj.o, 1, PART_FLARE, teamhint(proj.owner, col), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale, clamp(1.25f-proj.lifespan, 0.5f, 1.f)*0.25f);
                                part_flare(proj.to, proj.o, 1, PART_FLARE_LERP, col, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale*0.5f, clamp(1.25f-proj.lifespan, 0.5f, 1.f));
                            }
                        }
                        break;
                    }
                    case WEAP_SMG:
                    {
                        float size = clamp(WEAP2(proj.weap, partlen, proj.flags&HIT_ALT)*(1.f-proj.lifespan)*proj.curscale, proj.curscale, min(min(WEAP2(proj.weap, partlen, proj.flags&HIT_ALT), proj.movement), proj.o.dist(proj.from)));
                        if(size > 0)
                        {
                            proj.to = vec(proj.o).sub(vec(proj.vel).normalize().mul(size));
                            int col = ((int(224*max(1.f-proj.lifespan,0.3f))<<16))|((int(80*max(1.f-proj.lifespan,0.1f)))<<8);
                            part_flare(proj.to, proj.o, 1, PART_FLARE, teamhint(proj.owner, col), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale, clamp(1.25f-proj.lifespan, 0.5f, 1.f)*0.25f);
                            part_flare(proj.to, proj.o, 1, PART_FLARE_LERP, col, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale*0.5f, clamp(1.25f-proj.lifespan, 0.5f, 1.f));
                            if(!proj.child && WEAP2(proj.weap, flakweap, proj.flags&HIT_ALT) >= 0)
                            {
                                part_create(PART_PLASMA, 1, proj.o, col, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale*5.f);
                                part_create(PART_HINT_SOFT, 1, proj.o, teamhint(proj.owner, col), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*5.f*1.125f*proj.curscale, 0.5f);
                            }
                        }
                        break;
                    }
                    case WEAP_PLASMA:
                    {
                        float scale = lastmillis-proj.spawntime <= proj.lifemillis/10 ? (lastmillis-proj.spawntime)/float(proj.lifemillis/10) : 1,
                            expl = WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize*scale);
                        if(expl > 0)
                        {
                            part_explosion(proj.o, expl, PART_SHOCKBALL, 1, 0x55BBEE, 1.f, 0.25f);
                            if(WEAP(proj.weap, pusharea) >= 1)
                                part_explosion(proj.o, expl*WEAP(proj.weap, pusharea), PART_SHOCKWAVE, 1, teamhint(proj.owner, 0x55BBEE), 1.f, 0.1f);
                        }
                        part_create(PART_PLASMA_SOFT, 1, proj.o, 0x55BBEE, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.lifesize*proj.curscale*scale, proj.owner == game::focus ? 0.35f : 0.75f);
                        part_create(PART_ELECTRIC_SOFT, 1, proj.o, 0x55BBEE, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*0.45f*proj.lifesize*proj.curscale*scale, proj.owner == game::focus ? 0.35f : 0.75f);
                        part_create(PART_HINT_SOFT, 1, proj.o, teamhint(proj.owner, 0x55BBEE), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*1.125f*proj.lifesize*proj.curscale*scale, 0.5f);
                        break;
                    }
                    case WEAP_RIFLE:
                    {
                        float size = clamp(WEAP2(proj.weap, partlen, proj.flags&HIT_ALT)*(1.f-proj.lifespan)*proj.curscale, proj.curscale, min(min(WEAP2(proj.weap, partlen, proj.flags&HIT_ALT), proj.movement), proj.o.dist(proj.from)));
                        if(size > 0)
                        {
                            proj.to = vec(proj.o).sub(vec(proj.vel).normalize().mul(size));
                            part_flare(proj.to, proj.o, 1, PART_FLARE, teamhint(proj.owner, 0x6611FF), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale, 0.25f);
                            part_flare(proj.to, proj.o, 1, PART_FLARE_LERP, 0x6611FF, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale*0.5f);
                            if(!proj.child && WEAP2(proj.weap, flakweap, proj.flags&HIT_ALT) >= 0)
                            {
                                part_create(PART_PLASMA, 1, proj.o, 0x6611FF, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale*5.f);
                                part_create(PART_HINT_SOFT, 1, proj.o, teamhint(proj.owner, 0x6611FF), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*5.f*1.125f*proj.curscale, 0.5f);
                            }
                        }
                        break;
                    }
                    default: break;
                }
                break;
            }
            case PRJ_GIBS: case PRJ_DEBRIS:
            {
                if(proj.projtype == PRJ_GIBS && !kidmode && game::bloodscale > 0)
                {
                    if(proj.movement >= 1 && lastmillis-proj.lasteffect >= 1000 && proj.lifetime >= min(proj.lifemillis, proj.fadetime))
                    {
                        part_splash(PART_BLOOD, 1, game::bloodfade, proj.o, 0x229999, (rnd(game::bloodsize/2)+(game::bloodsize/2))/10.f, 1, 100, DECAL_BLOOD, int(proj.radius), 10);
                        proj.lasteffect = lastmillis - (lastmillis%1000);
                    }
                    if(!game::bloodsparks) break;
                }
                if(!proj.limited)
                {
                    bool effect = false;
                    float radius = (proj.radius+0.5f)*(clamp(1.f-proj.lifespan, 0.1f, 1.f)+0.25f), blend = clamp(1.25f-proj.lifespan, 0.25f, 1.f)*(0.75f+(rnd(25)/100.f)); // gets smaller as it gets older
                    if(projtrails && lastmillis-proj.lasteffect >= projtraildelay) { effect = true; proj.lasteffect = lastmillis - (lastmillis%projtraildelay); }
                    int len = effect ? max(int(projtraillength*0.5f*max(1.f-proj.lifespan, 0.1f)), 1) : 1;
                    part_create(PART_FIREBALL, len, proj.o, pulsecols[0][rnd(PULSECOLOURS)], radius, blend, -5);
                }
                break;
            }
            case PRJ_EJECT:
            {
                if(isweap(proj.weap) && ejecthint)
                    part_create(PART_HINT, 1, proj.o, weaptype[proj.weap].colour, max(proj.xradius, proj.yradius)*1.75f, clamp(1.f-proj.lifespan, 0.1f, 1.f)*0.35f);
                bool moving = proj.movement >= 1;
                if(moving && lastmillis-proj.lasteffect >= 100)
                {
                    part_create(PART_SMOKE, 75, proj.o, 0x222222, max(proj.xradius, proj.yradius)*1.75f, clamp(1.f-proj.lifespan, 0.1f, 1.f)*0.35f, -3);
                    proj.lasteffect = lastmillis - (lastmillis%100);
                }
            }
            case PRJ_AFFINITY:
            {
                bool moving = proj.movement >= 1;
                if(moving && lastmillis-proj.lasteffect >= 25)
                {
                    part_create(PART_SMOKE, 150, proj.o, 0xFFFFFF, max(proj.xradius, proj.yradius)*1.5f, 0.75f, -10);
                    proj.lasteffect = lastmillis - (lastmillis%100);
                }
            }
            default: break;
        }
    }

    void quake(const vec &o, int weap, int flags, float scale)
    {
        int q = int(WEAP2(weap, damage, flags&HIT_ALT)/float(WEAP2(weap, rays, flags&HIT_ALT))), r = int(WEAP2(weap, radius, flags&HIT_ALT)*WEAP(weap, pusharea)*scale);
        gameent *d;
        int numdyns = game::numdynents();
        loopi(numdyns) if((d = (gameent *)game::iterdynents(i)))
            d->quake = clamp(d->quake+max(int(q*max(1.f-d->o.dist(o)/r, 1e-3f)), 1), 0, 1000);
    }

    void destroy(projent &proj)
    {
        proj.lifespan = clamp((proj.lifemillis-proj.lifetime)/float(max(proj.lifemillis, 1)), 0.f, 1.f);
        switch(proj.projtype)
        {
            case PRJ_SHOT:
            {
                updatetargets(proj, 2);
                int vol = int(255*proj.curscale*(proj.child ? 0.25f : 1.f));
                if(proj.limited) vol = 0;
                else switch(proj.weap)
                {
                    case WEAP_PISTOL:
                    {
                        vol = int(vol*(1.f-proj.lifespan)*(proj.child ? 0.25f : 1.f));
                        part_create(PART_SMOKE_LERP, 200, proj.o, 0x999999, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale, 0.25f, -20);
                        float expl = WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        if(expl > 0)
                        {
                            quake(proj.o, proj.weap, proj.flags, proj.curscale);
                            part_explosion(proj.o, expl*0.5f, PART_EXPLOSION, 150, 0x884411, 1.f, 0.5f);
                            part_splash(PART_SPARK, 10, 500, proj.o, 0x884411, 0.125f, 1, 1, 0, expl, 10);
                            if(WEAP(proj.weap, pusharea) >= 1)
                                part_explosion(proj.o, expl*0.5f*WEAP(proj.weap, pusharea), PART_SHOCKWAVE, 75, teamhint(proj.owner, 0x884411), 1.f, 0.2f);
                            if(notrayspam(proj.weap, proj.flags&HIT_ALT, 0))
                            {
                                adddecal(DECAL_SCORCH_SHORT, proj.o, proj.norm, expl*0.5f);
                                adddynlight(proj.o, expl, vec(0.5f, 0.25f, 0.05f), 150, 10);
                            }
                        }
                        else if(notrayspam(proj.weap, proj.flags&HIT_ALT, 5)) adddecal(DECAL_BULLET, proj.o, proj.norm, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT));
                        break;
                    }
                    case WEAP_FLAMER: case WEAP_GRENADE: case WEAP_ROCKET:
                    { // both basically explosions
                        float expl = WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        if(proj.weap == WEAP_FLAMER)
                        {
                            if(expl <= 0) expl = WEAP2(proj.weap, partsize, proj.flags&HIT_ALT);
                            part_create(PART_SMOKE_LERP_SOFT, projtraillength, proj.o, 0x666666, expl*0.75f, 0.25f+(rnd(50)/100.f), -5);
                        }
                        else
                        {
                            part_create(PART_PLASMA_SOFT, projtraillength*(proj.weap != WEAP_ROCKET ? 2 : 3), proj.o, 0xAA4400, max(expl*0.5f, 0.5f)); // corona
                            if(expl > 0)
                            {
                                quake(proj.o, proj.weap, proj.flags, proj.curscale);
                                int len = proj.weap != WEAP_ROCKET ? 500 : 1000;
                                part_explosion(proj.o, expl, PART_EXPLOSION, len, 0xAA4400, 1.f, 0.5f);
                                part_splash(PART_SPARK, 30, len*3, proj.o, 0xAA4400, 0.25f, 1, 1, 0, expl, 20);
                                if(WEAP(proj.weap, pusharea) >= 1)
                                    part_explosion(proj.o, expl*WEAP(proj.weap, pusharea), PART_SHOCKWAVE, len/2, teamhint(proj.owner, 0xAA4400), 1.f, 0.2f);
                            }
                            else expl = WEAP2(proj.weap, partsize, proj.flags&HIT_ALT);
                            if(notrayspam(proj.weap, proj.flags&HIT_ALT, 1))
                            {
                                part_create(PART_SMOKE_LERP_SOFT, projtraillength*(proj.weap != WEAP_ROCKET ? 3 : 4), proj.o, 0x333333, expl*0.75f, 0.5f, -15);
                                int debris = rnd(proj.weap != WEAP_ROCKET ? 5 : 10)+5, amt = int((rnd(debris)+debris+1)*game::debrisscale);
                                loopi(amt) create(proj.o, vec(proj.o).add(proj.vel), true, proj.owner, PRJ_DEBRIS, rnd(game::debrisfade)+game::debrisfade, 0, rnd(501), rnd(101)+50);
                                adddecal(DECAL_ENERGY, proj.o, proj.norm, expl*0.75f, bvec(196, 24, 0));
                            }
                        }
                        if(notrayspam(proj.weap, proj.flags&HIT_ALT, 1))
                        {
                            int deviation = max(int(expl*0.5f), 1);
                            if(proj.weap != WEAP_FLAMER || proj.child || WEAP2(proj.weap, flakweap, proj.flags&HIT_ALT)%WEAP_MAX != WEAP_FLAMER)
                            {
                                loopi(proj.weap != WEAP_ROCKET ? 3 : 6)
                                {
                                    vec to(proj.o); loopk(3) to.v[k] += rnd(deviation*2)-deviation;
                                    part_create(PART_FIREBALL_SOFT, projtraillength*(proj.weap != WEAP_ROCKET ? 2 : 3), to, pulsecols[0][rnd(PULSECOLOURS)], expl, 0.25f+(rnd(25)/100.f), -5);
                                }
                            }
                            adddecal(proj.weap == WEAP_FLAMER ? DECAL_SCORCH_SHORT : DECAL_SCORCH, proj.o, proj.norm, expl*0.5f);
                            adddynlight(proj.o, expl, vec(1.1f, 0.22f, 0.02f), proj.weap != WEAP_ROCKET ? 500 : 1000, 10);
                        }
                        break;
                    }
                    case WEAP_SHOTGUN: case WEAP_SMG:
                    {
                        vol = int(vol*(1.f-proj.lifespan)*(proj.child ? 0.25f : 1.f));
                        part_splash(PART_SPARK, proj.weap == WEAP_SHOTGUN ? 10 : 20, 500, proj.o, proj.weap == WEAP_SMG ? 0xFF8822 : 0xFFAA22, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale*0.5f, 1, 1, 0, 16, 10);
                        float expl = WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        if(expl > 0)
                        {
                            quake(proj.o, proj.weap, proj.flags, proj.curscale);
                            part_explosion(proj.o, expl*0.5f, PART_EXPLOSION, 150, proj.weap == WEAP_SMG ? 0xAA4400 : 0xAA7711, 1.f, 0.5f);
                            if(WEAP(proj.weap, pusharea) >= 1)
                                part_explosion(proj.o, expl*0.5f*WEAP(proj.weap, pusharea), PART_SHOCKWAVE, 75, teamhint(proj.owner, proj.weap == WEAP_SMG ? 0xAA4400 : 0xAA7711), 1.f, 0.2f);
                            if(notrayspam(proj.weap, proj.flags&HIT_ALT, 0))
                            {
                                adddecal(DECAL_SCORCH_SHORT, proj.o, proj.norm, expl*0.5f);
                                adddynlight(proj.o, expl, vec(0.7f, proj.weap == WEAP_SMG ? 0.2f : 0.4f, 0.f), proj.flags&HIT_ALT ? 300 : 100, 10);
                            }
                        }
                        break;
                    }
                    case WEAP_PLASMA:
                    {
                        int len = proj.flags&HIT_ALT ? 500 : 250;
                        float expl = WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        if(expl > 0)
                        {
                            quake(proj.o, proj.weap, proj.flags, proj.curscale);
                            part_explosion(proj.o, expl, PART_SHOCKBALL, len, 0x55BBEE, 1.f, 0.25f);
                            if(WEAP(proj.weap, pusharea) >= 1)
                                part_explosion(proj.o, expl*WEAP(proj.weap, pusharea), PART_SHOCKWAVE, len/2, teamhint(proj.owner, 0x55BBEE), 1.f, 0.2f);
                        }
                        else expl = WEAP2(proj.weap, partsize, proj.flags&HIT_ALT);
                        part_splash(PART_SPARK, 20, len*3, proj.o, 0x55BBEE, 0.25f, 1, 1, 0, expl, 20);
                        part_create(PART_PLASMA_SOFT, len, proj.o, 0x55BBEE, expl*0.75f);
                        part_create(PART_ELECTRIC_SOFT, len/2, proj.o, 0x55BBEE, expl*0.375f);
                        part_create(PART_SMOKE, len, proj.o, 0x8896A4, expl*0.35f, 0.35f, -30);
                        if(notrayspam(proj.weap, proj.flags&HIT_ALT, 1))
                        {
                            adddecal(DECAL_ENERGY, proj.o, proj.norm, expl*0.75f, bvec(98, 196, 244));
                            adddynlight(proj.o, 1.1f*expl, vec(0.1f, 0.4f, 0.6f), len, 10);
                        }
                        break;
                    }
                    case WEAP_RIFLE:
                    {
                        float dist = proj.o.dist(proj.from), size = clamp(WEAP2(proj.weap, partlen, proj.flags&HIT_ALT)*proj.curscale, proj.curscale, min(WEAP2(proj.weap, partlen, proj.flags&HIT_ALT), proj.movement)),
                            expl = WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                        vec dir = dist >= size ? vec(proj.vel).normalize() : vec(proj.o).sub(proj.from).normalize();
                        proj.to = vec(proj.o).sub(vec(dir).mul(size));
                        int len = proj.flags&HIT_ALT ? 2000 : 1000;
                        if(expl > 0)
                        {
                            quake(proj.o, proj.weap, proj.flags, proj.curscale);
                            part_create(PART_PLASMA_SOFT, len, proj.o, 0x6611FF, expl*0.5f); // corona
                            part_splash(PART_SPARK, 20, len*3, proj.o, 0x6611FF, 0.25f, 1, 1, 0, expl, 15);
                            part_explosion(proj.o, expl, PART_SHOCKBALL, len, 0x6611FF, 1.f, 0.25f);
                            if(WEAP(proj.weap, pusharea) >= 1)
                                part_explosion(proj.o, expl*WEAP(proj.weap, pusharea), PART_SHOCKWAVE, len/2, teamhint(proj.owner, 0x6611FF), 1.f, 0.2f);
                        }
                        else expl = WEAP2(proj.weap, partsize, proj.flags&HIT_ALT);
                        part_flare(proj.to, proj.o, len, PART_FLARE, teamhint(proj.owner, 0x6611FF), WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale, 0.25f);
                        part_flare(proj.to, proj.o, len, PART_FLARE_LERP, 0x6611FF, WEAP2(proj.weap, partsize, proj.flags&HIT_ALT)*proj.curscale*0.5f);
                        if(notrayspam(proj.weap, proj.flags&HIT_ALT, 1))
                        {
                            adddecal(DECAL_SCORCH, proj.o, proj.norm, max(expl, 2.f));
                            adddecal(DECAL_ENERGY, proj.o, proj.norm, max(expl*0.5f, 1.f), bvec(98, 16, 254));
                            adddynlight(proj.o, 1.1f*expl, vec(0.4f, 0.05f, 1.f), len, 10);
                        }
                        break;
                    }
                    default: break;
                }
                if(vol > 0)
                {
                    int slot = WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize) > 0 ? S_W_EXPLODE : S_W_DESTROY;
                    playsound(WEAPSND2(proj.weap, proj.flags&HIT_ALT, slot), proj.o, NULL, 0, vol);
                }
                if(proj.owner)
                {
                    if(!proj.child && WEAP2(proj.weap, flakweap, proj.flags&HIT_ALT) >= 0)
                    {
                        int w = WEAP2(proj.weap, flakweap, proj.flags&HIT_ALT)%WEAP_MAX, life = WEAP2(proj.weap, flaktime, proj.flags&HIT_ALT);
                        float mag = max(proj.vel.magnitude(), 1.f), scale = WEAP2(proj.weap, flakscale, proj.flags&HIT_ALT)*proj.curscale,
                              skew = proj.hit ? WEAP2(proj.weap, flakskew, proj.flags&HIT_ALT) : WEAP2(proj.weap, flakspread, proj.flags&HIT_ALT);
                        vec pos = vec(proj.o).sub(vec(proj.vel).normalize().mul(proj.hit ? WEAP2(proj.weap, flakoffset, proj.flags&HIT_ALT) : 1));
                        if(WEAP2(proj.weap, flakffwd, proj.flags&HIT_ALT) > 0) life -= int(ceilf(life*WEAP2(proj.weap, flakffwd, proj.flags&HIT_ALT)));
                        loopi(WEAP2(proj.weap, flakrays, proj.flags&HIT_ALT))
                        {
                            vec dir(0, 0, 0);
                            if(WEAP2(proj.weap, flakspeed, proj.flags&HIT_ALT) > 0)
                                mag = rnd(WEAP2(proj.weap, flakspeed, proj.flags&HIT_ALT))*0.5f+WEAP2(proj.weap, flakspeed, proj.flags&HIT_ALT)*0.5f;
                            if(skew > 0) dir.add(vec(rnd(2001)-1000, rnd(2001)-1000, rnd(2001)-1000).normalize().mul(skew*mag));
                            if(WEAP2(proj.weap, flakrel, proj.flags&HIT_ALT) > 0)
                                dir.add(vec(proj.vel).normalize().mul(WEAP2(proj.weap, flakrel, proj.flags&HIT_ALT)*mag));
                            create(pos, dir.add(pos), proj.local, proj.owner, PRJ_SHOT, max(life, 1), WEAP2(proj.weap, flaktime, proj.flags&HIT_ALT), 0, WEAP2(proj.weap, flakspeed, proj.flags&HIT_ALT), proj.id, w, (WEAP2(proj.weap, flakweap, proj.flags&HIT_ALT) >= WEAP_MAX ? HIT_ALT : 0)|HIT_FLAK, scale, true, &proj);
                        }
                    }
                    if(proj.local)
                        client::addmsg(N_DESTROY, "ri8", proj.owner->clientnum, lastmillis-game::maptime, proj.weap, proj.flags, proj.child ? -proj.id : proj.id, 0, int(proj.curscale*DNF), 0);
                }
                break;
            }
            case PRJ_ENT:
            {
                if(proj.beenused <= 1 && proj.local && proj.owner)
                    client::addmsg(N_DESTROY, "ri8", proj.owner->clientnum, lastmillis-game::maptime, -1, 0, proj.id, 0, int(proj.curscale*DNF), 0);
                break;
            }
            case PRJ_AFFINITY:
            {
                if(proj.beenused <= 1) client::addmsg(N_RESETAFFIN, "ri", proj.id);
                if(m_capture(game::gamemode) && capture::st.flags.inrange(proj.id)) capture::st.flags[proj.id].proj = NULL;
                else if(m_bomber(game::gamemode) && bomber::st.flags.inrange(proj.id)) bomber::st.flags[proj.id].proj = NULL;
                break;
            }
            default: break;
        }
    }

    int check(projent &proj, const vec &dir)
    {
        if(proj.o.z < 0) return 0; // remove, always..
        int mat = lookupmaterial(vec(proj.o.x, proj.o.y, proj.o.z + (proj.aboveeye - proj.height)/2)), chk = 0;
        if(proj.extinguish&1 && int(mat&MATF_VOLUME) == MAT_WATER) chk |= 1;
        if(proj.extinguish&2 && (int(mat&MATF_VOLUME) == MAT_LAVA || int(mat&MATF_FLAGS) == MAT_DEATH)) chk |= 2;
        if(chk)
        {
            if(chk&1 && !proj.limited)
            {
                int vol = int(ceilf(48*proj.curscale*(proj.child ? 0.25f : 1.f))), snd = S_EXTINGUISH;
                float size = max(proj.radius, 1.f);
                if(proj.projtype == PRJ_SHOT && isweap(proj.weap))
                {
                    snd = WEAPSND2(proj.weap, proj.flags&HIT_ALT, S_W_EXTINGUISH);
                    vol = 10+int(245*proj.lifespan*proj.lifesize*proj.curscale*(proj.child ? 0.25f : 1.f));
                    float expl = WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                    if(expl > 0) size *= expl*1.5f;
                    else size *= 2.5f;
                }
                else size *= 2.5f;
                playsound(snd, proj.o, NULL, 0, vol);
                part_create(PART_SMOKE, 500, proj.o, 0xAAAAAA, max(size, 1.5f), 1, -10);
                proj.limited = true;
                if(proj.projtype == PRJ_DEBRIS) proj.light.material[0] = bvec(255, 255, 255);
            }
            proj.norm = dir;
            if(proj.extinguish&4) return 0;
        }
        return 1;
    }

    int impact(projent &proj, const vec &dir, physent *d, int flags, const vec &norm)
    {
        if(d && d->type == ENT_PROJ)
        {
            if(proj.projcollide&IMPACT_SHOTS)
            {
                proj.norm = vec(d->o).sub(proj.o).normalize();
                if(hiteffect(proj, d, flags, proj.norm)) return 0;
            }
            return 1;
        }
        if(d ? proj.projcollide&COLLIDE_PLAYER : proj.projcollide&COLLIDE_GEOM)
        {
            if(d)
            {
                proj.norm = vec(d->o).sub(proj.o).normalize();
                if(!hiteffect(proj, d, flags, proj.norm)) return 1;
            }
            else
            {
                proj.norm = norm;
                if(proj.projcollide&COLLIDE_STICK)
                {
                    proj.o.sub(vec(dir).mul(proj.radius*0.125f));
                    proj.stuck = true;
                    return 1;
                }
            }
            bool ricochet = proj.projcollide&(d ? BOUNCE_PLAYER : BOUNCE_GEOM);
            bounce(proj, ricochet);
            if(ricochet)
            {
                if(proj.projtype != PRJ_SHOT || proj.child || !weaptype[proj.weap].traced)
                {
                    reflect(proj, proj.norm);
                    proj.o.add(vec(proj.norm).mul(0.1f)); // offset from surface slightly to avoid initial collision
                    proj.movement = 0;
                }
                proj.lastbounce = lastmillis;
                return 2; // bounce
            }
            else if(proj.projcollide&(d ? IMPACT_PLAYER : IMPACT_GEOM))
                return 0; // die on impact
        }
        return 1; // live!
    }

    int step(projent &proj, const vec &dir)
    {
        int ret = check(proj, dir);
        if(ret == 1 && (!collide(&proj, dir, 0.f, proj.projcollide&COLLIDE_PLAYER) || inside))
            ret = impact(proj, dir, hitplayer, hitflags, wall);
        return ret;
    }

    int trace(projent &proj, const vec &dir)
    {
        int ret = check(proj, dir);
        if(ret == 1)
        {
            vec to(proj.o), ray = dir;
            to.add(dir);
            float maxdist = ray.magnitude();
            if(maxdist > 0)
            {
                ray.mul(1/maxdist);
                float dist = tracecollide(&proj, proj.o, ray, maxdist, RAY_CLIPMAT | RAY_ALPHAPOLY, proj.projcollide&COLLIDE_PLAYER);
                proj.o.add(vec(ray).mul(dist >= 0 ? dist : maxdist));
                if(dist >= 0) ret = impact(proj, dir, hitplayer, hitflags, hitsurface);
            }
        }
        return ret;
    }

    void escaped(projent &proj, const vec &pos, const vec &dir)
    {
        if(!(proj.projcollide&COLLIDE_OWNER) || proj.lastbounce) proj.escaped = true;
        else if(proj.spawntime && lastmillis-proj.spawntime >= (proj.projtype == PRJ_SHOT ? WEAP2(proj.weap, edelay, proj.flags&HIT_ALT) : PHYSMILLIS))
        {
            if(proj.projcollide&COLLIDE_TRACE)
            {
                vec to = vec(pos).add(dir);
                float x1 = floor(min(pos.x, to.x)), y1 = floor(min(pos.y, to.y)),
                      x2 = ceil(max(pos.x, to.x)), y2 = ceil(max(pos.y, to.y)),
                      maxdist = dir.magnitude(), dist = 1e16f;
                if(physics::xtracecollide(&proj, pos, to, x1, x2, y1, y2, maxdist, dist, proj.owner) || dist > maxdist) proj.escaped = true;
            }
            else if(physics::xcollide(&proj, dir, proj.owner)) proj.escaped = true;
        }
    }

    bool move(projent &proj, int qtime)
    {
        float secs = float(qtime)/1000.f;
        if(proj.projtype == PRJ_AFFINITY && m_bomber(game::gamemode) && proj.target && !proj.lastbounce)
        {
            vec targ = vec(proj.target->o).sub(proj.o).normalize();
            if(!targ.iszero())
            {
                vec dir = vec(proj.vel).normalize();
                float amt = clamp(bomberdelta*secs, 1e-8f, 1.f), mag = max(proj.vel.magnitude(), bomberminspeed);
                dir.mul(1.f-amt).add(targ.mul(amt)).normalize();
                if(!dir.iszero()) (proj.vel = dir).mul(mag);
            }
        }
        else if(proj.projtype == PRJ_SHOT && proj.escaped && proj.owner)
        {
            if(returningfire && lastmillis-proj.spawntime >= min(WEAP2(proj.weap, time, proj.flags&HIT_ALT)/3, 100))
            {
                if(!proj.stuck) proj.stuck = false;
                vec targ = vec(proj.owner->feetpos(proj.owner->height/2)).sub(proj.o).normalize();
                if(!targ.iszero()) proj.vel = vec(targ).mul(max(proj.vel.magnitude(), physics::movevelocity(&proj)));
            }
            else if(proj.owner->state == CS_ALIVE && WEAP2(proj.weap, guided, proj.flags&HIT_ALT) && lastmillis-proj.spawntime >= WEAP2(proj.weap, gdelay, proj.flags&HIT_ALT))
            {
                vec targ(0, 0, 0), dir = vec(proj.vel).normalize();
                switch(WEAP2(proj.weap, guided, proj.flags&HIT_ALT))
                {
                    case 5: case 4: case 3: case 2:
                    {
                        if(WEAP2(proj.weap, guided, proj.flags&HIT_ALT)%2 && proj.target && proj.target->state == CS_ALIVE)
                        {
                            targ = proj.target->headpos(-proj.target->height/2);
                            break;
                        }
                        physent *t = NULL;
                        switch(WEAP2(proj.weap, guided, proj.flags&HIT_ALT))
                        {
                            case 4: case 5:
                            {
                                float yaw, pitch;
                                vectoyawpitch(dir, yaw, pitch);
                                vec dest;
                                findorientation(proj.o, yaw, pitch, dest);
                                t = game::intersectclosest(proj.o, dest, proj.owner);
                                break;
                            }
                            case 2: case 3: default:
                            {
                                vec dest;
                                findorientation(proj.owner->o, proj.owner->yaw, proj.owner->pitch, dest);
                                t = game::intersectclosest(proj.owner->o, dest, proj.owner);
                                break;
                            }
                        }
                        if(t && (!m_team(game::gamemode, game::mutators) || t->type != ENT_PLAYER || ((gameent *)t)->team != proj.owner->team))
                        {
                            proj.target = t;
                            targ = proj.target->headpos(-proj.target->height/2);
                        }
                        break;
                    }
                    case 1: default: findorientation(proj.owner->o, proj.owner->yaw, proj.owner->pitch, targ); break;
                }
                if(!targ.iszero())
                {
                    float amt = clamp(WEAP2(proj.weap, delta, proj.flags&HIT_ALT)*secs, 1e-8f, 1.f),
                          mag = max(proj.vel.magnitude(), physics::movevelocity(&proj));
                    targ.sub(proj.o).normalize();
                    dir.mul(1.f-amt).add(targ.mul(amt)).normalize();
                    if(!dir.iszero()) (proj.vel = dir).mul(mag);
                }
            }
        }
        if(proj.weight != 0.f) proj.vel.z -= physics::gravityforce(&proj)*secs;

        vec dir(proj.vel), pos(proj.o);
        int mat = lookupmaterial(vec(proj.o.x, proj.o.y, proj.o.z + (proj.aboveeye - proj.height)/2));
        if(isliquid(mat&MATF_VOLUME) && proj.waterfric > 0) dir.div(proj.waterfric);
        dir.mul(secs);

        if(!proj.escaped && proj.owner) escaped(proj, pos, dir);

        bool blocked = false;
        if(proj.projcollide&COLLIDE_TRACE)
        {
            switch(trace(proj, dir))
            {
                case 2: blocked = true; break;
                case 1: break;
                case 0: return false;
            }
        }
        else
        {
            if(proj.projtype == PRJ_SHOT)
            {
                float stepdist = dir.magnitude();
                vec ray(dir);
                ray.mul(1/stepdist);
                float barrier = raycube(proj.o, ray, stepdist, RAY_CLIPMAT|RAY_POLY);
                if(barrier < stepdist)
                {
                    proj.o.add(ray.mul(barrier-0.15f));
                    switch(step(proj, ray))
                    {
                        case 2: proj.o = pos; blocked = true; break;
                        case 1: proj.o = pos; break;
                        case 0: return false;
                    }
                }
            }
            if(!blocked)
            {
                proj.o.add(dir);
                switch(step(proj, dir))
                {
                    case 2: proj.o = pos; if(proj.projtype == PRJ_SHOT) blocked = true; break;
                    case 1: default: break;
                    case 0: proj.o = pos; if(proj.projtype == PRJ_SHOT) { dir.rescale(max(dir.magnitude()-0.15f, 0.0f)); proj.o.add(dir); } return false;
                }
            }
        }

        float dist = proj.o.dist(pos), diff = dist/float(4*RAD);
        if(!blocked) proj.movement += dist;
        switch(proj.projtype)
        {
            case PRJ_SHOT:
            {
                if(proj.weap == WEAP_ROCKET)
                {
                    vectoyawpitch(vec(proj.vel).normalize(), proj.yaw, proj.pitch);
                    break;
                }
                if(proj.weap != WEAP_GRENADE) break;
            }
            case PRJ_DEBRIS: case PRJ_GIBS: case PRJ_AFFINITY:
            {
                if(!proj.lastbounce || proj.movement >= 1)
                {
                    float yaw = proj.yaw, pitch = proj.pitch;
                    vectoyawpitch(vec(proj.vel).normalize(), yaw, pitch);
                    game::scaleyawpitch(proj.yaw, proj.pitch, yaw, pitch, diff*secs);
                    vec axis(sinf(proj.yaw*RAD), -cosf(proj.yaw*RAD), 0);
                    if(proj.vel.dot2(axis) >= 0) { proj.roll -= diff; if(proj.roll < -180) proj.roll = 180 - fmod(180 - proj.roll, 360); }
                    else { proj.roll += diff; if(proj.roll > 180) proj.roll = fmod(proj.roll + 180, 360) - 180; }
                }
                break;
            }
            case PRJ_EJECT:
                if(!proj.lastbounce || proj.movement >= 1)
                {
                    vec axis(sinf(proj.yaw*RAD), -cosf(proj.yaw*RAD), 0);
                    if(proj.vel.dot2(axis) >= 0) { proj.pitch -= diff; if(proj.pitch < -180) proj.pitch = 180 - fmod(180 - proj.pitch, 360); }
                    else { proj.pitch += diff; if(proj.pitch > 180) proj.pitch = fmod(proj.pitch + 180, 360) - 180; }
                    break;
                }
            case PRJ_ENT:
            {
                if(proj.pitch != 0)
                {
                    if(proj.pitch < 0) { proj.pitch += max(diff, !proj.lastbounce || proj.movement >= 1 ? 1.f : 5.f); if(proj.pitch > 0) proj.pitch = 0; }
                    else if(proj.pitch > 0) { proj.pitch -= max(diff, !proj.lastbounce || proj.movement >= 1 ? 1.f : 5.f); if(proj.pitch < 0) proj.pitch = 0; }
                }
                break;
            }
            default: break;
        }
        return true;
    }

    bool moveframe(projent &proj)
    {
        if(((proj.lifetime -= physics::physframetime) <= 0 && proj.lifemillis) || (!proj.stuck && !proj.beenused && !move(proj, physics::physframetime)))
        {
            if(proj.lifetime < 0) proj.lifetime = 0;
            return false;
        }
        return true;
    }

    bool move(projent &proj)
    {
        if(physics::physsteps <= 0)
        {
            physics::interppos(&proj);
            return true;
        }

        bool alive = true;
        proj.o = proj.newpos;
        proj.o.z += proj.height;
        loopi(physics::physsteps-1) if(!(alive = moveframe(proj))) break;
        proj.deltapos = proj.o;
        if(alive) alive = moveframe(proj);
        proj.newpos = proj.o;
        proj.deltapos.sub(proj.newpos);
        proj.newpos.z -= proj.height;
        if(alive) physics::interppos(&proj);
        return alive;
    }

    bool raymove(projent &proj)
    {
        if((proj.lifetime -= curtime) <= 0 && proj.lifemillis)
        {
            if(proj.lifetime < 0) proj.lifetime = 0;
            return false;
        }
        vec to(proj.to), ray = vec(proj.to).sub(proj.from).mul(WEAP2(proj.weap, trace, proj.flags&HIT_ALT));
        float maxdist = ray.magnitude();
        if(maxdist <= 0) return 1; // not moving anywhere, so assume still alive since it was already alive
        ray.mul(1/maxdist);
        float dist = tracecollide(&proj, proj.from, ray, maxdist, RAY_CLIPMAT | RAY_ALPHAPOLY, proj.projcollide&COLLIDE_PLAYER);
        if(dist >= 0)
        {
            vec dir = vec(proj.to).sub(proj.from).normalize();
            proj.o = vec(proj.from).add(vec(dir).mul(dist));
            switch(impact(proj, dir, hitplayer, hitflags, hitsurface))
            {
                case 1: case 2: return true;
                case 0: default: return false;
            }
        }
        proj.o = proj.to;
        return true;
    }

    void update()
    {
        vector<projent *> canremove;
        loopvrev(projs) if(projs[i]->projtype == PRJ_DEBRIS || projs[i]->projtype == PRJ_GIBS || projs[i]->projtype == PRJ_EJECT)
            canremove.add(projs[i]);
        while(!canremove.empty() && canremove.length() > maxprojectiles)
        {
            int oldest = 0;
            loopv(canremove) if(canremove[i]->addtime < canremove[oldest]->addtime) oldest = i;
            if(canremove.inrange(oldest))
            {
                canremove[oldest]->state = CS_DEAD;
                canremove[oldest]->escaped = true;
                canremove.removeunordered(oldest);
            }
            else break;
        }

        loopv(projs)
        {
            projent &proj = *projs[i];
            if(proj.projtype == PRJ_SHOT && WEAP2(proj.weap, radial, proj.flags&HIT_ALT))
            {
                proj.hit = NULL;
                proj.hitflags = HITFLAG_NONE;
            }
            hits.setsize(0);
            if((proj.projtype != PRJ_SHOT || proj.owner) && proj.state != CS_DEAD)
            {
                if(proj.projtype == PRJ_ENT && entities::ents.inrange(proj.id)) // in case spawnweapon changes
                    proj.mdl = entities::entmdlname(entities::ents[proj.id]->type, entities::ents[proj.id]->attrs);
                if(proj.waittime > 0)
                {
                    if((proj.waittime -= curtime) <= 0)
                    {
                        proj.waittime = 0;
                        init(proj, true);
                    }
                    else continue;
                }
                iter(proj);
                if(proj.projtype == PRJ_SHOT || proj.projtype == PRJ_ENT || proj.projtype == PRJ_AFFINITY)
                {
                    if(proj.projtype == PRJ_SHOT && !proj.child && weaptype[proj.weap].traced ? !raymove(proj) : !move(proj)) switch(proj.projtype)
                    {
                        case PRJ_ENT: case PRJ_AFFINITY:
                        {
                            if(!proj.beenused)
                            {
                                proj.beenused = 1;
                                proj.lifetime = min(proj.lifetime, proj.fadetime);
                            }
                            if(proj.lifetime > 0) break;
                        }
                        default: proj.state = CS_DEAD; proj.escaped = true; break;
                    }
                }
                else for(int rtime = curtime; proj.state != CS_DEAD && rtime > 0;)
                {
                    int qtime = min(rtime, 30);
                    rtime -= qtime;

                    if(((proj.lifetime -= qtime) <= 0 && proj.lifemillis) || (!proj.stuck && !proj.beenused && !move(proj, qtime)))
                    {
                        if(proj.lifetime < 0) proj.lifetime = 0;
                        proj.state = CS_DEAD;
                        proj.escaped = true;
                        break;
                    }
                }
                effect(proj);
            }
            else
            {
                proj.state = CS_DEAD;
                proj.escaped = true;
            }
            if(m_bomber(game::gamemode) && proj.projtype == PRJ_AFFINITY && !proj.beenused && bomber::st.flags.inrange(proj.id))
            {
                gameent *d = bomber::st.flags[proj.id].lastowner;
                if(d && (d == game::player1 || d->ai))
                {
                    loopv(bomber::st.flags) if(!isbomberaffinity(bomber::st.flags[i]) && proj.o.dist(bomber::st.flags[i].spawnloc) <= enttype[AFFINITY].radius/2)
                    {
                        proj.beenused = 1;
                        proj.lifetime = min(proj.lifetime, proj.fadetime);
                        client::addmsg(N_SCOREAFFIN, "ri3", d->clientnum, proj.id, i);
                        break;
                    }
                }
            }
            if(proj.local && proj.owner && proj.projtype == PRJ_SHOT)
            {
                float expl = WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize);
                int radius = expl > 0 ? int(ceilf(expl)) : 0;
                if(!proj.child && weaptype[proj.weap].traced) proj.o = proj.to;
                if(proj.state == CS_DEAD)
                {
                    if(!(proj.projcollide&COLLIDE_CONT)) proj.hit = NULL;
                    if(!proj.limited && radius > 0) 
                    {
                        int numdyns = game::numdynents(true);
                        loopj(numdyns)
                        {
                            gameent *f = (gameent *)game::iterdynents(j, true);
                            if(!f || f->state != CS_ALIVE || !physics::issolid(f, &proj, false)) continue;
                            radialeffect(f, proj, true, radius);
                        }
                    }
                }
                else if(WEAP2(proj.weap, radial, proj.flags&HIT_ALT))
                {
                    if(!(proj.projcollide&COLLIDE_CONT)) proj.hit = NULL;
                    if(!proj.limited && radius > 0 && (!proj.lastradial || lastmillis-proj.lastradial >= 40)) 
                    {
                        int numdyns = game::numdynents();
                        loopj(numdyns)
                        {
                            gameent *f = (gameent *)game::iterdynents(j);
                            if(!f || f->state != CS_ALIVE || !physics::issolid(f, &proj, true)) continue;
                            if(radialeffect(f, proj, false, radius)) proj.lastradial = lastmillis;
                        }
                    }
                }
                if(!hits.empty())
                    client::addmsg(N_DESTROY, "ri7iv", proj.owner->clientnum, lastmillis-game::maptime, proj.weap, proj.flags, proj.child ? -proj.id : proj.id,
                            int(radius), int(proj.curscale*DNF), hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
            }
            if(proj.state == CS_DEAD)
            {
                destroy(proj);
                delete &proj;
                projs.removeunordered(i--);
            }
        }
    }

    void fadeproj(projent &proj, float &trans, float &size)
    {
        if(proj.fadetime && proj.lifemillis)
        {
            int interval = min(proj.lifemillis, proj.fadetime);
            if(proj.lifetime < interval)
            {
                float amt = float(proj.lifetime)/float(interval);
                size *= amt;
                trans *= amt;
            }
            else if(proj.projtype != PRJ_EJECT && proj.lifemillis > interval)
            {
                interval = min(proj.lifemillis-interval, proj.fadetime);
                if(proj.lifemillis-proj.lifetime < interval)
                {
                    float amt = float(proj.lifemillis-proj.lifetime)/float(interval);
                    size *= amt;
                    trans *= amt;
                }
            }
        }
    }

    void render()
    {
        loopv(projs) if(projs[i]->ready(false) && projs[i]->projtype != PRJ_AFFINITY)
        {
            projent &proj = *projs[i];
            if((proj.projtype == PRJ_ENT && !entities::ents.inrange(proj.id)) || !projs[i]->mdl || !*projs[i]->mdl) continue;
            float trans = 1, size = projs[i]->curscale, yaw = proj.yaw;
            int flags = MDL_CULL_VFC|MDL_CULL_OCCLUDED|MDL_LIGHT|MDL_CULL_DIST;
            switch(proj.projtype)
            {
                case PRJ_DEBRIS:
                {
                    if(shadowdebris) flags |= MDL_DYNSHADOW;
                    size *= proj.lifesize;
                    fadeproj(proj, trans, size);
                    break;
                }
                case PRJ_GIBS:
                {
                    if(shadowgibs) flags |= MDL_DYNSHADOW;
                    size *= proj.lifesize;
                    flags |= MDL_LIGHT_FAST;
                    fadeproj(proj, trans, size);
                    break;
                }
                case PRJ_EJECT:
                {
                    if(shadoweject) flags |= MDL_DYNSHADOW;
                    size *= proj.lifesize;
                    flags |= MDL_LIGHT_FAST;
                    fadeproj(proj, trans, size);
                    yaw += 90;
                    break;
                }
                case PRJ_SHOT:
                {
                    if(shadowents) flags |= MDL_DYNSHADOW;
                    yaw += 90;
                    break;
                }
                case PRJ_ENT:
                {
                    if(shadowents) flags |= MDL_DYNSHADOW;
                    fadeproj(proj, trans, size);
                    break;
                }
                default: break;
            }
            rendermodel(NULL, proj.mdl, ANIM_MAPMODEL|ANIM_LOOP, proj.o, yaw, proj.pitch, proj.roll, flags, &proj, NULL, 0, 0, trans, size);
        }
    }

    void adddynlights()
    {
        loopv(projs) if(projs[i]->ready() && projs[i]->projtype == PRJ_SHOT)
        {
            projent &proj = *projs[i];
            switch(proj.weap)
            {
                case WEAP_SWORD: adddynlight(proj.o, 16, vec(0.1f, 0.1f, 0.95f)); break;
                case WEAP_SHOTGUN: if(proj.movement >= 1) adddynlight(proj.o, 16*proj.curscale, vec(0.5f, 0.35f, 0.1f)); break;
                case WEAP_SMG: if(proj.movement >= 1) adddynlight(proj.o, 8*proj.curscale, vec(0.5f, 0.25f, 0.05f)); break;
                case WEAP_FLAMER:
                {
                    vec col(1.1f*max(1.f-proj.lifespan,0.3f)*proj.curscale, 0.2f*max(1.f-proj.lifespan,0.15f)*proj.curscale, 0.00f);
                    adddynlight(proj.o, 1.1f*WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize), col);
                    break;
                }
                case WEAP_PLASMA:
                {
                    vec col(0.1f*max(1.f-proj.lifespan,0.1f)*proj.curscale, 0.4f*max(1.f-proj.lifespan,0.1f)*proj.curscale, 0.6f*max(1.f-proj.lifespan,0.1f)*proj.curscale);
                    adddynlight(proj.o, 1.1f*WEAPEX(proj.weap, proj.flags&HIT_ALT, game::gamemode, game::mutators, proj.curscale*proj.lifesize), col);
                    break;
                }
                default: break;
            }
        }
    }
}
