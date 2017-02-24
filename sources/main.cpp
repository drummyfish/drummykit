/*
 * virtual 3D midi drumkit
 *
 * This is not my nicest code, but it was my first time with the engine
 * and I wanted to get something working fast, sorry ^^
 *
 * Miloslav Číž, 2014
 */

#include <irrlicht.h>
#include <iostream>
#include <string>
#include <ctime>
#include <vector>
#include <irrKlang.h>

extern "C" {
#include "midifile.h"
#include "midiinfo.h"
 }

using namespace irr;
using namespace std;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;
using namespace irrklang;

#ifdef _IRR_WINDOWS_
#pragma comment(lib,"Irrlicht.lib")
#pragma comment(linker,"/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#pragma comment(lib,"irrKlang.lib") // link with irrKlang.dll

#define VERSION_AUTHOR_YEAR L"v 1.1 by Miloslav Ciz, 2014"

#define RESOURCE_PATH "./resources/"
#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600
#define BAR_HEIGHT    80
                               // key mapping:
#define KEY_HIHAT_CLOSED       'q'
#define KEY_HIHAT_OPENED       'w'
#define KEY_HIHAT_PEDAL        'e'
#define KEY_BASS               'r'
#define KEY_BASS_2             't'
#define KEY_SNARE              'u'
#define KEY_SNARE_RIM          'i'
#define KEY_SNARE_2            'o'
#define KEY_TOM1               'l'
#define KEY_TOM2               'k'
#define KEY_TOM3               'j'
#define KEY_TOM4               'h'
#define KEY_TOM5               'g'
#define KEY_TOM6               'f'
#define KEY_CRASH              'p'
#define KEY_CRASH_2            'n'
#define KEY_SPLASH             'm'
#define KEY_CHINA              'b'
#define KEY_RIDE               'd'
#define KEY_RIDE_BELL          'x'
#define KEY_RIDE_2             'c'
#define KEY_COWBELL            's'
#define KEY_TAMBOURINE         'a'

unsigned int current_time;    /// current time in milliseconds

enum    // GUI elements IDs
  {
    ID_CHECKBOX_HIGHLIGHTS = 101,
    ID_CHECKBOX_ANIMATIONS,
    ID_CHECKBOX_SOUNDS,
    ID_EDITBOX_PATH,
    ID_EDITBOX_TRACK,
    ID_EDITBOX_TEMPO,
    ID_BUTTON_LOAD,
    ID_BUTTON_PLAY,
    ID_SCROLLBAR
  };

typedef struct    ///< holds the settings
  {
    bool display_highlights;
    bool display_animations;
    bool play_sounds;
  } t_settings;

typedef struct    ///< represents a drum hit event
  {
    double time;                  // time in seconds from the start when the hit happens
    unsigned int time_midi;       // time in midi units
    unsigned int strength;       // strength of the hit
    unsigned char drum;           // drum being hit, should be used with constants defined above
  } t_drum_hit;

typedef struct   ///< holds the player properties
  {
    double current_time;          // current time in seconds
    double max_time;
    unsigned int current_note;    // current index of note waiting to be played
    char play_note;               // which note should be played (code)
    int play_strength;            // strength to be played (0,1 or 2)
    bool playing;
  } t_player;

//-------------------------------

class c_drum                        ///< represents a drum
  {
    public:
      c_drum(ISceneManager* scene_manager, ISoundEngine *sound_engine, t_settings *settings,
        std::string name, std::string texture_name, std::string sound1, std::string sound2)
        {
          std::string helper_string;
          this->sound_playing = NULL;
          this->name = name;
          this->sound_engine = sound_engine;

          this->settings = settings;

          this->sound1 = sound1;
          this->sound2 = sound2;

          helper_string = RESOURCE_PATH + texture_name + " texture.png";
          this->texture = scene_manager->getVideoDriver()->getTexture(helper_string.c_str());

          helper_string = RESOURCE_PATH + texture_name + " texture highlighted.png";
          this->texture_highlighted = scene_manager->getVideoDriver()->getTexture(helper_string.c_str());

          helper_string = RESOURCE_PATH + texture_name + " texture highlighted2.png";
          this->texture_highlighted2 = scene_manager->getVideoDriver()->getTexture(helper_string.c_str());

          helper_string = RESOURCE_PATH + name + ".md2";
          this->node = scene_manager->addAnimatedMeshSceneNode(scene_manager->getMesh(helper_string.c_str()));

          if (this->node)
            {
              this->node->setMaterialFlag(EMF_LIGHTING,false);
              this->node->setMaterialTexture(0,this->texture);
            }

          this->node->setFrameLoop(0,0);    // don't animate
          this->node->setLoopMode(false);
          this->highlight_end_time = -1;
        }

      virtual void animate(unsigned int animation_no)
        {
          if (animation_no != 0)    // basic drums only have maximum of 1 animation
            return;

          this->node->setMD2Animation(EMAT_STAND);
          this->node->setAnimationSpeed(80);
          this->node->setLoopMode(false);
          this->node->setFrameLoop(0,this->node->getEndFrame());
        }

      virtual void play_sound(unsigned int sound_no, unsigned int strength)
        /**<
         * Plays given sound (0 or 1) with given strength (0, 1 or 2)
         */

        {
          std::string helper_string;
          char helper_char;


          if (this->sound_playing != NULL)  // stop the already playing sound
            {
              this->sound_playing->stop();
              this->sound_playing->drop();
              this->sound_playing = NULL;
            }

          helper_char = '0';
          helper_char += strength;

          if (sound_no == 0)
            {
              helper_string = RESOURCE_PATH + this->sound1 + helper_char + ".wav";
              this->sound_engine->play2D(helper_string.c_str(),false);
            }
          else
            {
              if (this->sound2.length() != 0)
                {
                  helper_string = RESOURCE_PATH + this->sound2 + helper_char + ".wav";
                  this->sound_engine->play2D(helper_string.c_str(),false);
                }
            }
        }

      void update()
        /**<
         * Updates the drum, should be called every frame.
         */

        {
          if (this->highlight_end_time < 0)
            return;

          if (current_time >= (unsigned int) highlight_end_time)
            {
              this->node->setMaterialTexture(0,this->texture);
              this->highlight_end_time = -1;
            }
        }

      void highlight(unsigned int milliseconds, unsigned int highlight_no)

        /**<
         * Highlights the drum with given highlight for given number of
         * milliseconds.
         */

        {
          if (highlight_no == 0 || this->texture_highlighted2 == NULL)
            this->node->setMaterialTexture(0,this->texture_highlighted);
          else
            this->node->setMaterialTexture(0,this->texture_highlighted2);

          this->highlight_end_time = current_time + milliseconds;
        }

      void hit(unsigned int hit_no, unsigned int strength)

        /**<
         * Should be called when the drum is hit.
         */

        {
          if (this->settings->display_highlights)
            this->highlight(100,hit_no);

          if (this->settings->display_animations)
            this->animate(hit_no);

          if (this->settings->play_sounds)
            this->play_sound(hit_no,strength);
        }

    protected:
      t_settings *settings;
      std::string name,sound1,sound2;
      IAnimatedMeshSceneNode *node;
      ISoundEngine *sound_engine;
      ITexture *texture;
      ITexture *texture_highlighted;
      ITexture *texture_highlighted2; // another optional highlight (such as rim)
      long highlight_end_time;        // negative means no highlight
      ISound *sound_playing;
  };

//-------------------------------

class c_hihat_drum: public c_drum   ///< hi-hat drum
  {
    public:
      c_hihat_drum(ISceneManager* scene_manager, ISoundEngine *sound_engine, t_settings *settings,
        std::string name, std::string texture_name, std::string sound1, std::string sound2):
        c_drum(scene_manager,sound_engine,settings,name,texture_name,sound1,sound2) {}

      void play_sound(unsigned int sound_no, unsigned int strength)

          {
            std::string helper_string;
            char helper_char;

            helper_char = '0';
            helper_char += strength;

            if (this->sound_playing != NULL)  // stop the already playing sound
              {
                this->sound_playing->stop();
                this->sound_playing->drop();
                this->sound_playing = NULL;
              }

            if (sound_no != 1)
              {
                helper_string = RESOURCE_PATH + this->sound1 + helper_char + ".wav";
                this->sound_playing = this->sound_engine->play2D(helper_string.c_str(),false,false,true);
              }
            else
              {
                if (this->sound2.length() != 0)
                  {
                    helper_string = RESOURCE_PATH + this->sound2 + helper_char + ".wav";
                    this->sound_playing = this->sound_engine->play2D(helper_string.c_str(),false,false,true);
                  }
              }
          }

        void animate(unsigned int animation_no)
          {
            this->node->setMD2Animation(EMAT_STAND);
            this->node->setAnimationSpeed(120);
            this->node->setLoopMode(false);

            if (animation_no == 0)
              this->node->setFrameLoop(0,30);
            else if (animation_no == 1)
              this->node->setFrameLoop(30,60);
            else
              this->node->setFrameLoop(60,this->node->getEndFrame());
          }
  };

//-------------------------------

class c_bass_drum: public c_drum    ///< bass drum
  {
    public:
      c_bass_drum(ISceneManager* scene_manager, ISoundEngine *sound_engine, t_settings *settings,
        std::string name, std::string texture_name, std::string sound1, std::string sound2):
        c_drum(scene_manager,sound_engine,settings,name,texture_name,sound1,sound2) {}

      void animate(unsigned int animation_no)
        {
          this->node->setMD2Animation(EMAT_STAND);
          this->node->setAnimationSpeed(200);
          this->node->setLoopMode(false);

          if (animation_no == 0)
            this->node->setFrameLoop(0,50);
          else
            this->node->setFrameLoop(50,this->node->getEndFrame());
        }
  };

//-------------------------------

class c_drumset /// a drumset
  {
    public:
      c_drum *snare;
      c_drum *snare2;
      c_drum *crash1;
      c_drum *crash2;
      c_drum *splash;
      c_hihat_drum *hihat;
      c_drum *china;
      c_drum *ride1;
      c_drum *ride2;
      c_drum *tom1;
      c_drum *tom2;
      c_drum *tom3;
      c_drum *tom4;
      c_drum *tom5;
      c_drum *tom6;
      c_drum *tambourine;
      c_drum *cowbell;
      c_bass_drum *bass;
      t_settings settings;

      c_drumset (ISceneManager* scene_manager, ISoundEngine* sound_engine)
        {
          this->settings.display_animations = true;
          this->settings.display_highlights = true;
          this->settings.play_sounds = true;

          this->snare = new c_drum(scene_manager,sound_engine,&this->settings,"snare","snare","snare","rim");
          this->snare2 = new c_drum(scene_manager,sound_engine,&this->settings,"snare2","snare2","snare2","");
          this->crash1 = new c_drum(scene_manager,sound_engine,&this->settings,"crash","cymbal","crash1","");
          this->crash2 = new c_drum(scene_manager,sound_engine,&this->settings,"crash2","cymbal","crash2","");
          this->splash = new c_drum(scene_manager,sound_engine,&this->settings,"splash","cymbal","splash","");
          this->hihat = new c_hihat_drum(scene_manager,sound_engine,&this->settings,"hihat","cymbal4","closedhihat","openhihat");
          this->china = new c_drum(scene_manager,sound_engine,&this->settings,"china","cymbal3","china","");
          this->ride1 = new c_drum(scene_manager,sound_engine,&this->settings,"ride","cymbal2","ride","bell");
          this->ride2 = new c_drum(scene_manager,sound_engine,&this->settings,"ride2","cymbal2","ride2","");
          this->tom1 = new c_drum(scene_manager,sound_engine,&this->settings,"tom1","tom","tom1","");
          this->tom2 = new c_drum(scene_manager,sound_engine,&this->settings,"tom2","tom","tom2","");
          this->tom3 = new c_drum(scene_manager,sound_engine,&this->settings,"tom3","tom","tom3","");
          this->tom4 = new c_drum(scene_manager,sound_engine,&this->settings,"tom4","tom","tom4","");
          this->tom5 = new c_drum(scene_manager,sound_engine,&this->settings,"tom5","tom2","tom5","");
          this->tom6 = new c_drum(scene_manager,sound_engine,&this->settings,"tom6","tom2","tom6","");
          this->tambourine = new c_drum(scene_manager,sound_engine,&this->settings,"tambourine","percussion","tambourine","");
          this->cowbell = new c_drum(scene_manager,sound_engine,&this->settings,"cowbell","percussion","cowbell","");
          this->bass = new c_bass_drum(scene_manager,sound_engine,&this->settings,"bass","bass","bass","bass");
        }

      ~c_drumset()
        {
          delete this->snare;
          delete this->snare2;
          delete this->crash1;
          delete this->crash2;
          delete this->splash;
          delete this->hihat;
          delete this->china;
          delete this->ride1;
          delete this->ride2;
          delete this->tom1;
          delete this->tom2;
          delete this->tom3;
          delete this->tom4;
          delete this->tom5;
          delete this->tom6;
          delete this->tambourine;
          delete this->cowbell;
          delete this->bass;
        }

      void update()
        /**<
         * Updates all drums, should be called each frame.
         */

        {
          this->snare->update();
          this->snare2->update();
          this->crash1->update();
          this->crash2->update();
          this->splash->update();
          this->hihat->update();
          this->china->update();
          this->ride1->update();
          this->ride2->update();
          this->tom1->update();
          this->tom2->update();
          this->tom3->update();
          this->tom4->update();
          this->tom5->update();
          this->tom6->update();
          this->tambourine->update();
          this->cowbell->update();
          this->bass->update();
        }
  };


//-------------------------------

class c_event_receiver: public IEventReceiver  /// holds the keys being pressed and mouse coordinations, also handles some events
  {
    public:
      bool load_pressed;             /// whether the load button has been pressed
      bool play_pressed;
      bool mouse_left, mouse_right;
      bool keys[256];                /// indexed by lowercase char, says which keys are pressed
      bool keys_pressed[256];        /// to capture only single press
      int mouse_x, mouse_y;
      bool scrollbar_changed;
      t_settings *settings;

      c_event_receiver()
        {
          unsigned char c;

          this->settings = NULL;
          this->load_pressed = false;
          this->play_pressed = false;
          this->scrollbar_changed = false;

          for (c = 0; c < 255; c++)
            {
              this->keys[c] = false;
              this->keys_pressed[c] = false;
            }

          this->mouse_x = 0;
          this->mouse_y = 0;
        }

      void set_settings_pointer(t_settings *settings)
        /**<
         * Sets a pointer to settings struct that will be modified by this
         * receiver when GUI events happen.
         */

        {
          this->settings = settings;
        }

      bool load_was_pressed()
        {
          if (this->load_pressed)
            {
              this->load_pressed = false;  // reset the value to false
              return true;
            }

          return false;
        }

      bool play_was_pressed()
        {
          if (this->play_pressed)
            {
              this->play_pressed = false;
              return true;
            }

          return false;
        }

      int scrollbar_dragged()

        {
          if (this->scrollbar_changed)
            {
              this->scrollbar_changed = false;
              return true;
            }

          return false;
        }

      bool was_pressed(char key)

        /**<
         * Checks if one key was pressed.
         */

        {
          if (this->mouse_y < BAR_HEIGHT)  // to prevent playing when typing
            return false;

          if (this->keys[key])
            {
              if (!this->keys_pressed[key])
                {
                  this->keys_pressed[key] = true;
                  return true;
                }
              else
                return false;
            }
          else
            {
              this->keys_pressed[key] = false;
              return false;
            }
        }

      virtual bool OnEvent(const SEvent& event)
        {
          if (event.EventType == EET_KEY_INPUT_EVENT)
            {
              switch (event.KeyInput.Key)
                {
                  case KEY_KEY_A: this->keys['a'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_B: this->keys['b'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_C: this->keys['c'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_D: this->keys['d'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_E: this->keys['e'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_F: this->keys['f'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_G: this->keys['g'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_H: this->keys['h'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_I: this->keys['i'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_J: this->keys['j'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_K: this->keys['k'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_L: this->keys['l'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_M: this->keys['m'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_N: this->keys['n'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_O: this->keys['o'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_P: this->keys['p'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_Q: this->keys['q'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_R: this->keys['r'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_S: this->keys['s'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_T: this->keys['t'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_U: this->keys['u'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_V: this->keys['v'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_W: this->keys['w'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_X: this->keys['x'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_Y: this->keys['y'] = event.KeyInput.PressedDown; break;
                  case KEY_KEY_Z: this->keys['z'] = event.KeyInput.PressedDown; break;
                  default: break;
                }
            }
          else if (event.EventType == EET_MOUSE_INPUT_EVENT)
            {
              mouse_left = event.MouseInput.isLeftPressed();
              mouse_right = event.MouseInput.isRightPressed();
              mouse_x = event.MouseInput.X;
              mouse_y = event.MouseInput.Y;
            }
          else if (event.EventType == EET_GUI_EVENT)
            {
              s32 id = event.GUIEvent.Caller->getID();

              switch (id)
                {
                  case ID_CHECKBOX_ANIMATIONS:
                    this->settings->display_animations = ((IGUICheckBox *)event.GUIEvent.Caller)->isChecked();
                    break;

                  case ID_CHECKBOX_HIGHLIGHTS:
                    this->settings->display_highlights = ((IGUICheckBox *)event.GUIEvent.Caller)->isChecked();
                    break;

                  case ID_CHECKBOX_SOUNDS:
                    this->settings->play_sounds = ((IGUICheckBox *)event.GUIEvent.Caller)->isChecked();
                    break;

                  case ID_BUTTON_LOAD:
                    if (event.GUIEvent.EventType == EGET_BUTTON_CLICKED)
                      this->load_pressed = true;
                    break;

                  case ID_BUTTON_PLAY:
                    if (event.GUIEvent.EventType == EGET_BUTTON_CLICKED)
                      this->play_pressed = true;
                    break;

                  case ID_SCROLLBAR:
                    if (event.GUIEvent.EventType == EGET_SCROLL_BAR_CHANGED)
                      this->scrollbar_changed = true;
                    break;
                }
            }

          return false;
      }
};

//----------------------------------------------------------------------

const wchar_t* strToWchart(std::string sInput) // code snippet from Irrlicht forums
  {
    wchar_t* wCharOutput = new wchar_t[1023];

    const char* cStr;
    cStr = sInput.c_str();
    mbstowcs(wCharOutput,cStr,strlen(cStr)+1);

    return wCharOutput;
  }

//----------------------------------------------------------------------

const std::string wchartToStr(const wchar_t* wInput) // code snippet from Irrlicht forums
  {
    std::string sOutput = "";
    char* cOut = new char[1023];

    wcstombs(cOut,wInput,1023);
    sOutput += cOut;

    return sOutput;
  }

//----------------------------------------------------------------------

vector<t_drum_hit> make_hit_vector(std::string filename, int track_number, bool *success)

  /**<
   * Makes a vector of drum hits representing a midi song.
   *
   * @param filename midi file
   * @param track_number track to be loaded, or -1 to auto detect
   * @param success in this variable the success state will be returned
   */

  {
    vector<t_drum_hit> song;
    t_drum_hit hit;
    MIDI_FILE *midi_file = NULL;
    MIDI_MSG midi_message;
    int current_tempo;
    bool do_insert;
    int ppqn; // see the wiki for PPQN
    unsigned int i, j, number_of_tracks;
    double current_dt;   // delta time
    bool load_this_track;  // for auto detecting drum tracks

    *success = false;

    midi_file = midiFileOpen(filename.c_str());

    if (!midi_file)
      return song;

    midiReadInitMessage(&midi_message);

    number_of_tracks = midiReadGetNumTracks(midi_file);
    ppqn = midiFileGetPPQN(midi_file);

    for (i = 0; i < number_of_tracks; i++)   // read everything in all tracks
      {
        load_this_track = false;

        while (midiReadGetNextMessage(midi_file,i,&midi_message))
          {
            do_insert = false;

            if (midi_message.iType == msgMetaEvent && midi_message.MsgData.MetaEvent.iType == metaSetTempo)
              {
                hit.drum = 0;         // let the zero represent tempo change event
                hit.time_midi = midi_message.dwAbsPos;
                hit.strength = midi_message.MsgData.MetaEvent.Data.Tempo.iBPM;
                do_insert = true;
              }
            else if (track_number < 0 && midi_message.iType == msgSetProgram)
              {
                if (midi_message.MsgData.ChangeProgram.iChannel == MIDI_CHANNEL_DRUMS)
                  load_this_track = true;
              }
            else if ((i == (unsigned int) track_number || load_this_track) && midi_message.iType == msgNoteOn)  // track and note we're interested in
              {
                switch (midi_message.MsgData.NoteOn.iNote)
                  {
                    case 35: hit.drum = KEY_BASS_2; break;
                    case 36: hit.drum = KEY_BASS; break;
                    case 37: hit.drum = KEY_SNARE_RIM; break;
                    case 38: hit.drum = KEY_SNARE; break;
                    case 39: hit.drum = KEY_SNARE_RIM; break;     // normally clap
                    case 40: hit.drum = KEY_SNARE_2; break;
                    case 41: hit.drum = KEY_TOM6; break;
                    case 42: hit.drum = KEY_HIHAT_CLOSED; break;
                    case 43: hit.drum = KEY_TOM5; break;
                    case 44: hit.drum = KEY_HIHAT_PEDAL; break;
                    case 45: hit.drum = KEY_TOM4; break;
                    case 46: hit.drum = KEY_HIHAT_OPENED; break;
                    case 47: hit.drum = KEY_TOM3; break;
                    case 48: hit.drum = KEY_TOM2; break;
                    case 49: hit.drum = KEY_CRASH; break;
                    case 50: hit.drum = KEY_TOM1; break;
                    case 51: hit.drum = KEY_RIDE; break;
                    case 52: hit.drum = KEY_CHINA; break;
                    case 53: hit.drum = KEY_RIDE_BELL; break;
                    case 54: hit.drum = KEY_TAMBOURINE; break;
                    case 55: hit.drum = KEY_SPLASH; break;
                    case 56: hit.drum = KEY_COWBELL; break;
                    case 57: hit.drum = KEY_CRASH_2; break;
                    case 58: hit.drum = KEY_COWBELL; break;      // normally vibraslap
                    case 59: hit.drum = KEY_RIDE_2; break;
                    case 60: hit.drum = KEY_TOM3; break;         // normally hi bongo
                    case 61: hit.drum = KEY_TOM4; break;         // normally low bongo
                    case 62: hit.drum = KEY_TOM1; break;         // normally mute hi bongo
                    case 63: hit.drum = KEY_TOM2; break;         // normally mute low bongo
                    case 64: hit.drum = KEY_TOM5; break;         // normally low conga
                    case 65: hit.drum = KEY_TOM3; break;         // normally hi timbale
                    case 66: hit.drum = KEY_TOM1; break;         // normally low timbale
                    case 67: hit.drum = KEY_COWBELL; break;      // normally hi agogo
                    case 68: hit.drum = KEY_COWBELL; break;      // normally low agogo
                    case 69: hit.drum = KEY_HIHAT_OPENED; break; // normally cabasa
                    case 70: hit.drum = KEY_HIHAT_OPENED; break; // normally maracas
                    case 71: hit.drum = KEY_RIDE_BELL; break;    // normally short whistle
                    case 72: hit.drum = KEY_RIDE_BELL; break;    // normally long whistle
                    case 75: hit.drum = KEY_SNARE_RIM; break;    // normally claves
                    case 76: hit.drum = KEY_SNARE_RIM; break;    // normally hi wood block
                    case 77: hit.drum = KEY_SNARE_RIM; break;    // normally low wood block
                    case 78: hit.drum = KEY_TOM6; break;         // normally mute cuica
                    case 79: hit.drum = KEY_TOM5; break;         // normally open cuica
                    case 80: hit.drum = KEY_RIDE_BELL; break;    // normally open triangle
                    case 81: hit.drum = KEY_RIDE_BELL; break;    // normally mute triangle

                    default: hit.drum = 1; break;   // nothing
                  }

                hit.strength = midi_message.MsgData.NoteOn.iVolume;
                hit.time_midi = midi_message.dwAbsPos;
                do_insert = true;
              }

            if (do_insert)      // place the event right so the vector stays sorted
              {
                j = 0;

                while (j < song.size() && song.at(j).time_midi < hit.time_midi)
                  j++;

                if (j >= song.size())
                  song.push_back(hit);
                else
                  song.insert(song.begin() + j,hit);
              }
          }
        }

    current_tempo = 120;
    current_dt = 60.0 / (ppqn * current_tempo);
    int previous_time_midi = 0;
    double previous_time = 0.0;

    for (i = 0; i < song.size(); i++)  // compute the real times
      {
        if (song.at(i).drum == 0)      // tempo change
          {
            current_tempo = song.at(i).strength;
            current_dt = 60.0 / (ppqn * current_tempo);
          }

        song.at(i).time = previous_time + current_dt * (song.at(i).time_midi - previous_time_midi);

        previous_time = song.at(i).time;
        previous_time_midi = song.at(i).time_midi;
      }

    midiFileClose(midi_file);

    *success = true;
    return song;
  }

//----------------------------------------------------------------------

int main()
  {
    c_event_receiver event_receiver;
    ICameraSceneNode *camera;
    IAnimatedMeshSceneNode *helper_node;
    vector3d<float> helper_vector;
    ISceneNodeAnimator *fps_animator;
    std::string helper_string;
    vector<t_drum_hit> song;
    double time_before;             // time in the last frame
    bool success;
    t_player player;
    int i, strength;
    char drum_code;
    c_drum *helper_drum;
    unsigned char helper_hit_number;
    bool loop_marking;

    success = false;
    player.playing = false;
    time_before = 0.0;

    ISoundEngine* sound_engine = createIrrKlangDevice();
    IrrlichtDevice *device = createDevice(video::EDT_OPENGL,dimension2d<u32>(WINDOW_WIDTH,WINDOW_HEIGHT),16,false,false,false,&event_receiver);
	IVideoDriver* driver = device->getVideoDriver();
	ISceneManager* scene_manager = device->getSceneManager();
	IGUIEnvironment* gui = device->getGUIEnvironment();
    ITimer *timer = device->getTimer();

    device->setWindowCaption(L"Drummykit");

    // set up the GUI:

    IGUISkin *new_skin = gui->createSkin(EGST_BURNING_SKIN);
    new_skin->setColor(EGDC_WINDOW,SColor(200,100,100,100));
    new_skin->setColor(EGDC_BUTTON_TEXT,SColor(200,100,255,0));
    new_skin->setColor(EGDC_EDITABLE,SColor(200,120,120,120));
    new_skin->setColor(EGDC_FOCUSED_EDITABLE,SColor(200,150,150,150));
    new_skin->setColor(EGDC_HIGH_LIGHT_TEXT,SColor(200,180,180,180));

    IGUIFont* font = gui->getFont(((std::string) RESOURCE_PATH + "fontcourier.bmp").c_str());

    if (font)
      new_skin->setFont(font);

    gui->setSkin(new_skin);

    IGUIWindow *bar = gui->addWindow(rect<s32>(0,0,WINDOW_WIDTH,BAR_HEIGHT));
    bar->setDraggable(false);
    bar->setDrawTitlebar(false);
    bar->getCloseButton()->remove();
    gui->addCheckBox(true,rect<s32>(20,10,70,60),bar,ID_CHECKBOX_HIGHLIGHTS);
    gui->addCheckBox(true,rect<s32>(80,10,130,60),bar,ID_CHECKBOX_ANIMATIONS);
    gui->addCheckBox(true,rect<s32>(140,10,190,60),bar,ID_CHECKBOX_SOUNDS);
    IGUIEditBox *editbox_filename = gui->addEditBox(L"",rect<s32>(210,25,400,45),true,bar,ID_EDITBOX_PATH);
    gui->addButton(rect<s32>(470,25,520,45),bar,ID_BUTTON_LOAD,L"load");
    IGUIEditBox *editbox_track = gui->addEditBox(L"a",rect<s32>(410,25,450,45),true,bar,ID_EDITBOX_TRACK);
    IGUIScrollBar *scroll_bar = gui->addScrollBar(true,rect<s32>(570,25,750,45),bar,ID_SCROLLBAR);
    IGUIButton *button_play = gui->addButton(rect<s32>(765,25,785,45),bar,ID_BUTTON_PLAY,L">");

    scroll_bar->setMin(0);
    scroll_bar->setMax(100);

    (gui->addStaticText(L"high.",rect<s32>(20,55,300,150),false,true,bar))->setOverrideColor(SColor(255,100,255,0));
    (gui->addStaticText(L"anim.",rect<s32>(80,55,360,150),false,true,bar))->setOverrideColor(SColor(255,100,255,0));
    (gui->addStaticText(L"sounds",rect<s32>(140,55,480,150),false,true,bar))->setOverrideColor(SColor(255,100,255,0));
    (gui->addStaticText(L"filename",rect<s32>(210,55,540,150),false,true,bar))->setOverrideColor(SColor(255,100,255,0));
    (gui->addStaticText(L"track",rect<s32>(410,55,600,150),false,true,bar))->setOverrideColor(SColor(255,100,255,0));
    (gui->addStaticText(VERSION_AUTHOR_YEAR,rect<s32>(550,55,800,150),false,true,bar))->setOverrideColor(SColor(255,255,255,255));

    // create the drumset:

    c_drumset drumset(scene_manager,sound_engine);
    event_receiver.set_settings_pointer(&drumset.settings);

    // load the floor model:

    helper_node = scene_manager->addAnimatedMeshSceneNode(scene_manager->getMesh(((std::string) RESOURCE_PATH + "floor.md2").c_str()));

    if (helper_node)
      {
        helper_node->setMaterialFlag(EMF_LIGHTING,false);
        helper_node->setMaterialTexture(0,driver->getTexture(((std::string) RESOURCE_PATH + (std::string) "floor.png").c_str()));
      }

	camera = scene_manager->addCameraSceneNodeFPS(0,200.0f,0.1f,-1);

    helper_vector.set(-6,24,-39);
    camera->setPosition(helper_vector);

    fps_animator = *(camera->getAnimators().getLast());
    fps_animator->grab();    // so that the animator doesn't get deleted when removed from camera

	while(device->run())     // the main loop
	  {
        if (event_receiver.mouse_left && event_receiver.mouse_y > BAR_HEIGHT) // handle the camera
          {
            if (camera->getAnimators().getSize() == 0)
              {
                camera->addAnimator(fps_animator);
                device->getCursorControl()->setVisible(false);
              }
          }
        else
          {
            camera->removeAnimators();
            device->getCursorControl()->setVisible(true);
          }

        if (event_receiver.load_was_pressed())
          {
            std::string helper_string = wchartToStr(editbox_track->getText());
            int track_number;

            if (helper_string.compare("a") == 0)
              track_number = -1;                   // auto detect track
            else
              track_number = atoi(helper_string.c_str());

            song = make_hit_vector(wchartToStr(editbox_filename->getText()),track_number,&success);

            if (success)
              {
                player.playing = false;
                button_play->setText(L">");
                player.current_time = 0;
                player.current_note = 0;
                player.play_note = 0;
                scroll_bar->setPos(0);
                player.max_time = song.size() != 0 ? song.at(song.size() - 1).time + 0.1 : 0.0;
                gui->addMessageBox(L"message",L"song and track loaded succesfully");
              }
            else
              gui->addMessageBox(L"message",L"error loading file");
          }

        if (event_receiver.play_was_pressed() && success)
          {
            if (player.current_time < player.max_time)
              player.playing = !player.playing;

            if (player.playing)
              button_play->setText(L"||");
            else
              button_play->setText(L">");
          }

        if (event_receiver.scrollbar_dragged())
          {
            player.current_time = player.max_time * (scroll_bar->getPos() / 100.0);

            for (i = 0; (unsigned int) i < song.size(); i++)    // seek the current note
              {
                if (song.at(i).time > player.current_time)
                  break;
              }

            player.current_note = i;
          }

        loop_marking = true;

        do   // mark all the drums to be played in this frame
          {
            loop_marking = false;

            if (player.playing)     // handle the player
              {
                scroll_bar->setPos((s32) (player.current_time / player.max_time * 100));

                if (player.current_time >= player.max_time || player.current_note >= song.size())  // end of the song
                  {
                    player.playing = false;
                    button_play->setText(L">");
                  }
                else
                  {
                    player.current_time += (current_time - time_before) / 1000.0;

                    if (player.current_time >= song.at(player.current_note).time)
                      {
                        player.play_note = song.at(player.current_note).drum;
                        player.play_strength = song.at(player.current_note).strength;

                        if (player.play_strength < 75)
                          player.play_strength = 0;
                        else if (player.play_strength < 115)
                          player.play_strength = 1;
                        else
                          player.play_strength = 2;

                        player.current_note++;

                        if (player.current_note < song.size() && song.at(player.current_note).time >= player.current_time)
                          loop_marking = true;    // multiple notes playing at once => do this loop again
                      }
                    else
                      player.play_note = 0;
                  }
              }

            time_before = current_time;
            current_time = timer->getRealTime(); // update the time count

            // playing the notes keyboard:

            for (i = 0; i < 23; i++)
              {
                switch (i)
                  {
                    case 0: drum_code = KEY_HIHAT_CLOSED; helper_drum = drumset.hihat; helper_hit_number = 0; break;
                    case 1: drum_code = KEY_HIHAT_OPENED; helper_drum = drumset.hihat; helper_hit_number = 1; break;
                    case 2: drum_code = KEY_HIHAT_PEDAL; helper_drum = drumset.hihat; helper_hit_number = 2; break;
                    case 3: drum_code = KEY_BASS; helper_drum = drumset.bass; helper_hit_number = 0; break;
                    case 4: drum_code = KEY_BASS_2; helper_drum = drumset.bass; helper_hit_number = 1; break;
                    case 5: drum_code = KEY_SNARE; helper_drum = drumset.snare; helper_hit_number = 0; break;
                    case 6: drum_code = KEY_SNARE_RIM; helper_drum = drumset.snare; helper_hit_number = 1; break;
                    case 7: drum_code = KEY_SNARE_2; helper_drum = drumset.snare2; helper_hit_number = 0; break;
                    case 8: drum_code = KEY_TOM1; helper_drum = drumset.tom1; helper_hit_number = 0; break;
                    case 9: drum_code = KEY_TOM2; helper_drum = drumset.tom2; helper_hit_number = 0; break;
                    case 10: drum_code = KEY_TOM3; helper_drum = drumset.tom3; helper_hit_number = 0; break;
                    case 11: drum_code = KEY_TOM4; helper_drum = drumset.tom4; helper_hit_number = 0; break;
                    case 12: drum_code = KEY_TOM5; helper_drum = drumset.tom5; helper_hit_number = 0; break;
                    case 13: drum_code = KEY_TOM6; helper_drum = drumset.tom6; helper_hit_number = 0; break;
                    case 14: drum_code = KEY_RIDE; helper_drum = drumset.ride1; helper_hit_number = 0; break;
                    case 15: drum_code = KEY_RIDE_BELL; helper_drum = drumset.ride1; helper_hit_number = 1; break;
                    case 16: drum_code = KEY_RIDE_2; helper_drum = drumset.ride2; helper_hit_number = 0;  break;
                    case 17: drum_code = KEY_COWBELL; helper_drum = drumset.cowbell; helper_hit_number = 0;  break;
                    case 18: drum_code = KEY_TAMBOURINE; helper_drum = drumset.tambourine; helper_hit_number = 0;  break;
                    case 19: drum_code = KEY_CRASH; helper_drum = drumset.crash1; helper_hit_number = 0; break;
                    case 20: drum_code = KEY_CRASH_2; helper_drum = drumset.crash2; helper_hit_number = 0; break;
                    case 21: drum_code = KEY_SPLASH; helper_drum = drumset.splash; helper_hit_number = 0; break;
                    case 22: drum_code = KEY_CHINA; helper_drum = drumset.china; helper_hit_number = 0; break;
                    default: helper_drum = NULL; break;
                  }

                if (helper_drum != NULL)
                  {
                    if (event_receiver.was_pressed(drum_code))
                      helper_drum->hit(helper_hit_number,1);
                    else if (player.playing && player.play_note == drum_code)
                      helper_drum->hit(helper_hit_number,player.play_strength);
                  }
              }
          } while (loop_marking);

        // update drums:

        drumset.update();

        // draw the scene

    	driver->beginScene(true,true,SColor(255,0,0,0));
		scene_manager->drawAll();
		gui->drawAll();
		driver->endScene();
	  }

    sound_engine->drop();
	device->drop();
    fps_animator->drop();
	return 0;
}

//----------------------------------------------------------------------
