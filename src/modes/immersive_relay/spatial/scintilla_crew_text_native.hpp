// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_CREW_TEXT_NATIVE_HPP
#define HEADER_SCINTILLA_CREW_TEXT_NATIVE_HPP
// Included by the owning engine. Native shaping and project fonts supply real
// glyphs; bounded in-memory authoring happens only while the world is held.
namespace {
#ifndef SERVER_ONLY
video::ITexture* crewTextAtlas=nullptr; // STKTexManager owns it, as for SC4 results.
std::string crewTextKey;
std::vector<std::string> crewTextParticipantIDs;
std::vector<std::string> crewTextParticipantNames;
enum CrewTextCell {CrewName=0,CrewRole=6,CrewTask=12,CrewKart=19,CrewCapsule=51,
    CrewTether=75,CrewBoost=76,CrewParty=77,CrewReason=78,CrewPointer=97,
    CrewChange=98,CrewPrepare=99,CrewReady=100,CrewRelease=101,CrewWait=102,
    CrewResultStandard=110,CrewResultParty=111,CrewResultContributions=112,
    CrewResultTurns=113,CrewResultTurbo=114,CrewResultComparable=115,
    CrewResultTest=116};
core::stringw crewWords(const char* en,const char* it)
{
    // Use the native language selection and gettext fallback. Italian venue
    // wording is explicit until upstream catalogs contain these new strings.
    const auto language=translations?translations->getCurrentLanguageNameCode():"en";
    return language.compare(0,2,"it")==0?StringUtils::utf8ToWide(it):translations?_(en):StringUtils::utf8ToWide(en);
}
struct CrewGlyphCollector:FontWithFace::FontCharCollector {
    struct Glyph {video::ITexture* texture;core::rect<float> dest;core::rect<s32> source;};
    std::vector<Glyph> glyphs;bool overflow=false;
    void collectChar(video::ITexture* t,const core::rect<float>& d,const core::rect<s32>& s,const video::SColor* const)override
    {if(glyphs.size()>=8192){overflow=true;return;}glyphs.push_back({t,d,s});}
};
bool prepareCrewTextAtlas(std::string* reason)
{
    auto fail=[&](const char* r){if(reason)*reason=r;return false;};
    missionAssetsInvalidated=true;
    if(!RelaySessionController::get()->isWorldHeld())return fail("crew_text_requires_held_world");
    const auto people=RelaySessionController::get()->getSpatialParticipants();
    if(people.size()<2||people.size()>6){crewTextAtlas=nullptr;crewTextKey.clear();return fail("crew_text_roster_unavailable");}
    std::string key=std::to_string(raceEpoch)+"|"+(translations?translations->getCurrentLanguageNameCode():"en");
    for(const auto& p:people){if(p.m_display_name.size()>256){crewTextAtlas=nullptr;crewTextKey.clear();return fail("crew_text_name_size");}key+='|';key+=p.m_player_id;key+='|';key+=p.m_display_name;}
    if(auto* world=World::getWorld())for(unsigned i=0;i<world->getNumKarts();++i)if(auto* kart=world->getKart(i)){key+='|';key+=StringUtils::wideToUtf8(kart->getName());}
    key=missionSHA256(key);if(crewTextAtlas&&crewTextKey==key)return true;
    crewTextAtlas=nullptr;crewTextKey.clear();
    if(!font_manager)return fail("crew_text_font_unavailable");
    auto* face=font_manager->getFont<RegularFace>();if(!face)return fail("crew_text_font_unavailable");
    if(irr_driver->getVideoDriver()->getDriverType()!=video::EDT_OPENGL)return fail("crew_text_renderer_unavailable");
    if(!missionResultAtlas)return fail("crew_text_result_atlas_unavailable");
    std::array<core::stringw,134> labels;
    for(unsigned i=0;i<people.size();++i)labels[128+i]=labels[i]=StringUtils::utf8ToWide(people[i].m_display_name);
    const char* rolesEN[]={"Universal","Hacker","Gunner","Defender","Engineer","Recon"};
    const char* rolesIT[]={"Universale","Hacker","Cannoniere","Difensore","Ingegnere","Ricognitore"};
    for(unsigned i=0;i<6;++i)labels[CrewRole+i]=crewWords(rolesEN[i],rolesIT[i]);
    const char* tasksEN[]={"Open / hit","Defend","Open shell","Collect","Recover","Boost","Crew turbo"};
    const char* tasksIT[]={"Apri / colpisci","Difendi","Apri guscio","Raccogli","Recupera","Boost","Turbo crew"};
    for(unsigned i=0;i<7;++i)labels[CrewTask+i]=crewWords(tasksEN[i],tasksIT[i]);
    for(unsigned i=0;i<4;++i){ // SC5's admitted native profile: one human + three AI.
        auto* world=World::getWorld();auto* kart=world&&i<world->getNumKarts()?world->getKart(i):nullptr;
        labels[CrewKart+i]=kart?kart->getName():crewWords("Kart ","Kart ")+StringUtils::toWString(i+1);
    }
    for(unsigned i=0;i<24;++i)labels[CrewCapsule+i]=crewWords("Capsule ","Capsula ")+StringUtils::toWString(i+1);
    labels[CrewTether]=crewWords("Cargo tether","Cavo del carico");labels[CrewBoost]=crewWords("Boost node","Nodo boost");labels[CrewParty]=crewWords("Crew turbo","Turbo crew");
    const char* reasonsEN[]={"Ready","Off","Prepare crew","Used this turn","Reserve protected","Expired: release","Release and press","Driver brake","Zone unavailable","Zone unqualified","Service cooldown","Boost priority","Defend first","Reinforcing","Native boost","Kart unavailable","Tracking lost","Check pointer","Service busy"};
    const char* reasonsIT[]={"Pronto","Spento","Prepara crew","Usato nel turno","Riserva protetta","Scaduto: rilascia","Rilascia e ripremi","Freno del pilota","Zona non ammessa","Zona da verificare","Attendi cooldown","Priorità boost","Prima difendi","Rinforzo attivo","Boost nativo","Kart non pronto","Tracking perso","Verifica puntatore","Servizio occupato"};
    for(unsigned i=0;i<19;++i)labels[CrewReason+i]=crewWords(reasonsEN[i],reasonsIT[i]);
    labels[CrewPointer]=crewWords("Check pointer","Verifica puntatore");labels[CrewChange]=crewWords("Change driver","Cambio pilota");
    labels[CrewPrepare]=crewWords("Preparing","Preparazione");labels[CrewReady]=crewWords("Driver: Ready","Pilota: Pronto");
    labels[CrewRelease]=crewWords("Release and press","Rilascia e ripremi");labels[CrewWait]=crewWords("Watch the dome","Osserva il dome");
    labels[103]=crewWords("Front","Davanti");labels[104]=crewWords("Right","Destra");labels[105]=crewWords("Rear","Dietro");labels[106]=crewWords("Left","Sinistra");
    labels[107]=crewWords("Open shield","Apri scudo");labels[108]=crewWords("Hit","Colpisci");
    labels[109]=crewWords("Keep holding","Mantieni il fascio");
    labels[117]=crewWords("Wait for breach","Attendi la breccia");
    labels[118]=crewWords("Contribution counted","Contributo registrato");
    labels[119]=crewWords("Release to defend","Rilascia per difendere");
    const char* statusEN[]={"Target busy","Cargo full","Shell open","Use tool","Collected","Recovered","Cargo lost","Rejected","Reserve protected","Energy empty","Too hot","Wrong sector","Cooldown","Defend first","Kart unavailable","Blocked","Reinforcing","Hit","Miss","Tracking lost"};
    const char* statusIT[]={"Bersaglio occupato","Carico pieno","Guscio aperto","Usa il fascio","Raccolto","Recuperato","Carico perso","Rifiutato","Riserva protetta","Energia esaurita","Troppo calore","Settore errato","Attendi cooldown","Prima difendi","Kart non pronto","Bloccato","Rinforzo attivo","Colpito","Mancato","Tracking perso"};
    for(unsigned i=0;i<20;++i)labels[23+i]=crewWords(statusEN[i],statusIT[i]);
    labels[43]=crewWords("Opportunity passed","Opportunità passata");labels[44]=crewWords("Contribution","Contributo");
    labels[45]=crewWords("Rival stopped","Rivale fermato");labels[46]=crewWords("Rebound","Rimbalzo");labels[47]=crewWords("Waiting","In attesa");
    labels[CrewResultStandard]=crewWords("SC5 standard crew","SC5 crew standard");
    labels[CrewResultParty]=crewWords("SC5 Party crew","SC5 crew Party");
    labels[CrewResultContributions]=crewWords("Crew contributions","Contributi della crew");
    labels[CrewResultTurns]=crewWords("Driver changes","Cambi pilota");
    labels[CrewResultTurbo]=crewWords("Crew turbos","Turbo crew");
    labels[CrewResultComparable]=crewWords("Comparable result","Risultato confrontabile");
    labels[CrewResultTest]=crewWords("Test / not comparable","Test / non confrontabile");
    auto* driver=irr_driver->getVideoDriver();auto* atlas=driver->createImage(video::ECF_A8R8G8B8,core::dimension2du(2048,2048));
    if(!atlas)return fail("crew_text_image_unavailable");atlas->fill(video::SColor(0,255,255,255));
    struct Page {video::IImage* image=nullptr;bool red=false;};std::map<video::ITexture*,Page> pages;uint64_t pagePixels=0;bool okay=true;
    // Glyph generation/lock may touch texture unpack state. Preserve the
    // caller's state rather than relying on a window-sized FontDrawer RTT.
    GLint activeUnit=0,bound=0,unpack=0,row=0,skipRows=0,skipPixels=0,unpackBuffer=0;
    glGetIntegerv(GL_ACTIVE_TEXTURE,&activeUnit);glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);
    GLState packState;glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);glPixelStorei(GL_PACK_SWAP_BYTES,GL_FALSE);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    // The shared sampler contains both pre-shaped crew text (top 1024 rows)
    // and the unchanged five SC4 outcome rows. Copy only while held; freezing
    // or rendering a result performs no glyph generation or file access.
    const auto outcomeSize=missionResultAtlas->getSize();
    if(outcomeSize.Width!=2048||outcomeSize.Height!=800)okay=false;
    void* outcomeData=okay?missionResultAtlas->lock(video::ETLM_READ_ONLY):nullptr;
    if(outcomeData){
        auto* outcomes=driver->createImageFromData(video::ECF_A8R8G8B8,outcomeSize,outcomeData,false);
        missionResultAtlas->unlock();
        if(outcomes){outcomes->copyTo(atlas,core::position2di(0,1024));outcomes->drop();}
        else okay=false;
    }else okay=false;
    // Shape all labels before taking any page snapshot: later glyph uploads
    // into the same page must not leave the in-memory copy missing new letters.
    CrewGlyphCollector warm;
    for(const auto& label:labels)if(!label.empty())face->drawText(label+L"…",core::rect<s32>(0,0,4096,256),video::SColor(255,255,255,255),false,false,nullptr,nullptr,&warm);
    if(warm.overflow)okay=false;
    for(unsigned cell=0;okay&&cell<labels.size();++cell){if(labels[cell].empty())continue;
        const bool wideName=cell>=128;const int cellWidth=wideName?1024:256;
        const int cellX=wideName?int((cell-128)%2)*1024:int(cell%8)*256;
        const int cellY=wideName?1840+int((cell-128)/2)*64:int(cell/8)*64;
        const unsigned maxWidth=unsigned(cellWidth-10);
        FontSettings settings;const auto natural=face->getDimension(labels[cell]);
        settings.setScale(std::min(1.f,44.f/std::max(1u,natural.Height)));
        if(cell>=6&&!wideName)settings.setScale(std::min(settings.getScale(),float(maxWidth)/std::max(1u,natural.Width)));
        // Long names are shaped again after truncation; the ellipsis is a real
        // native glyph and all samples stay inside this cell's inner padding.
        core::stringw label=labels[cell];while(label.size()>1&&face->getDimension(label,&settings).Width>maxWidth){label=label.subString(0,label.size()-1);}
        if(label!=labels[cell]){while(label.size()>1&&face->getDimension(label+L"…",&settings).Width>maxWidth)label=label.subString(0,label.size()-1);label+=L"…";}
        const auto size=face->getDimension(label,&settings);CrewGlyphCollector chars;
        face->drawText(label,core::rect<s32>(0,0,size.Width,size.Height),video::SColor(255,255,255,255),false,false,nullptr,&settings,&chars);
        if(chars.overflow){okay=false;break;}
        const int ox=cellX+(cellWidth-int(size.Width))/2,oy=cellY+(64-int(size.Height))/2;
        for(const auto& glyph:chars.glyphs){auto it=pages.find(glyph.texture);
            if(it==pages.end()){
                if(!glyph.texture||pages.size()>=16){okay=false;break;}
                const auto dimensions=glyph.texture->getSize();pagePixels+=uint64_t(dimensions.Width)*dimensions.Height;
                if(pagePixels>16u*1024u*1024u){okay=false;break;}
                void* data=glyph.texture->lock(video::ETLM_READ_ONLY);if(!data){okay=false;break;}
                // GL's R8 getTexImage expands raw R to red, not its sampler
                // swizzle. Its red channel is coverage; RGBA glyphs use alpha.
                glBindTexture(GL_TEXTURE_2D,glyph.texture->getTextureHandler());GLint format=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
                Page page;page.red=format==GL_R8;page.image=driver->createImageFromData(video::ECF_A8R8G8B8,dimensions,data,false);
                glyph.texture->unlock();if(!page.image){okay=false;break;}it=pages.emplace(glyph.texture,page).first;
            }
            const auto& g=glyph;const int left=int(std::floor(g.dest.UpperLeftCorner.X)),top=int(std::floor(g.dest.UpperLeftCorner.Y));
            const int width=int(std::ceil(g.dest.LowerRightCorner.X))-left,height=int(std::ceil(g.dest.LowerRightCorner.Y))-top;
            const auto dim=it->second.image->getDimension();if(width<=0||height<=0||g.source.getWidth()<=0||g.source.getHeight()<=0)continue;
            if(g.source.UpperLeftCorner.X<0||g.source.UpperLeftCorner.Y<0||g.source.LowerRightCorner.X>int(dim.Width)||g.source.LowerRightCorner.Y>int(dim.Height)){okay=false;break;}
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){const int dx=ox+left+x,dy=oy+top+y;
                if(dx<cellX+3||dx>=cellX+cellWidth-3||dy<cellY+3||dy>=cellY+64-3)continue;
                const unsigned sx=g.source.UpperLeftCorner.X+std::min(g.source.getWidth()-1,x*g.source.getWidth()/width),sy=g.source.UpperLeftCorner.Y+std::min(g.source.getHeight()-1,y*g.source.getHeight()/height);
                const auto pixel=it->second.image->getPixel(sx,sy);const unsigned alpha=it->second.red?pixel.getRed():pixel.getAlpha();
                if(alpha>atlas->getPixel(dx,dy).getAlpha())atlas->setPixel(dx,dy,video::SColor(alpha,255,255,255),false);
            }
        }
    }
    for(auto& page:pages)page.second.image->drop();
    // Exactly one cached atlas: repeated races or changed participant names
    // cannot accumulate a new GPU texture per roster or handoff.
    auto* manager=STKTexManager::getInstance();
    if(manager->hasTexture("scintilla-crew-text")){
        auto* old=manager->getTexture("scintilla-crew-text");const auto retired=old->getTextureHandler();
        // A caller may have left this atlas bound. Never restore a deleted GL
        // name after replacement; all surviving projection bindings are kept.
        if(GLuint(bound)==retired){bound=0;packState.texture=0;}
        manager->removeTexture(old);
    }
    video::ITexture* texture=nullptr;
    if(okay)texture=GE::createTexture(atlas,"scintilla-crew-text"); // OpenGL constructor consumes IImage.
    else atlas->drop();
    if(texture&&texture->getTextureHandler()){
        glBindTexture(GL_TEXTURE_2D,texture->getTextureHandler());GLint width=0,height=0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
        okay=width==2048&&height==2048;
    }else okay=false;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);glPixelStorei(GL_UNPACK_ALIGNMENT,unpack);glPixelStorei(GL_UNPACK_ROW_LENGTH,row);glPixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels);glActiveTexture(activeUnit);glBindTexture(GL_TEXTURE_2D,bound);
    if(!okay){if(texture)texture->drop();return fail("crew_text_atlas_unavailable");}
    crewTextAtlas=STKTexManager::getInstance()->addTexture(texture);crewTextKey=key;
    crewTextParticipantIDs.clear();crewTextParticipantNames.clear();for(const auto& person:people){
        crewTextParticipantIDs.push_back(person.m_player_id);crewTextParticipantNames.push_back(person.m_display_name);}
    return true;
}
void releaseCrewTextAtlas(const void* texture)
{if(!texture||texture==crewTextAtlas){crewTextAtlas=nullptr;crewTextKey.clear();crewTextParticipantIDs.clear();crewTextParticipantNames.clear();
    if(crew.required()){missionAssetsInvalidated=true;if(auto* controller=RelaySessionController::getExisting())controller->setCrewStartState(true,false);}}}
bool crewHUDAssetsReady(){return crewTextAtlas&&!crewTextKey.empty()&&!missionAssetsInvalidated;}
#else
bool crewHUDAssetsReady(){return false;}
#endif
}
#endif
