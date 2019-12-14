
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <assert.h>
#include <float.h>


#include "minorGems/util/stringUtils.h"
#include "minorGems/util/SettingsManager.h"
#include "minorGems/util/SimpleVector.h"
#include "minorGems/network/SocketServer.h"
#include "minorGems/network/SocketPoll.h"
#include "minorGems/network/web/WebRequest.h"
#include "minorGems/network/web/URLUtils.h"

#include "minorGems/crypto/hashes/sha1.h"

#include "minorGems/system/Thread.h"
#include "minorGems/system/Time.h"

#include "minorGems/game/doublePair.h"

#include "minorGems/util/log/AppLog.h"
#include "minorGems/util/log/FileLog.h"

#include "minorGems/formats/encodingUtils.h"

#include "minorGems/io/file/File.h"


#include "map.h"
#include "../gameSource/transitionBank.h"
#include "../gameSource/objectBank.h"
#include "../gameSource/objectMetadata.h"
#include "../gameSource/animationBank.h"
#include "../gameSource/categoryBank.h"

#include "lifeLog.h"
#include "foodLog.h"
#include "backup.h"
#include "triggers.h"
#include "playerStats.h"
#include "lineageLog.h"
#include "serverCalls.h"
#include "failureLog.h"
#include "names.h"
#include "curses.h"
#include "lineageLimit.h"
#include "objectSurvey.h"
#include "language.h"
#include "familySkipList.h"
#include "lifeTokens.h"
#include "fitnessScore.h"
#include "arcReport.h"
#include "curseDB.h"
#include "specialBiomes.h"


#include "minorGems/util/random/JenkinsRandomSource.h"


//#define IGNORE_PRINTF

#ifdef IGNORE_PRINTF
#define printf(fmt, ...) (0)
#endif


static FILE *familyDataLogFile = NULL;


static JenkinsRandomSource randSource;


#include "../gameSource/GridPos.h"


#define HEAT_MAP_D 13

float targetHeat = 10;


double secondsPerYear = 60.0;


#define NUM_BADGE_COLORS 17


#define PERSON_OBJ_ID 12


int minPickupBabyAge = 10;

int babyAge = 5;

// age when bare-hand actions become available to a baby (opening doors, etc.)
int defaultActionAge = 3;

// can't walk for first 12 seconds
double startWalkingAge = 0.20;



double forceDeathAge = 60;


double minSayGapInSeconds = 1.0;

// for emote throttling
double emoteWindowSeconds = 60.0;
int maxEmotesInWindow = 10;

double emoteCooldownSeconds = 120.0;



int maxLineageTracked = 20;

int apocalypsePossible = 0;
char apocalypseTriggered = false;
char apocalypseRemote = false;
GridPos apocalypseLocation = { 0, 0 };
int lastApocalypseNumber = 0;
double apocalypseStartTime = 0;
char apocalypseStarted = false;
char postApocalypseStarted = false;


double remoteApocalypseCheckInterval = 30;
double lastRemoteApocalypseCheckTime = 0;
WebRequest *apocalypseRequest = NULL;



char monumentCallPending = false;
int monumentCallX = 0;
int monumentCallY = 0;
int monumentCallID = 0;




static double minFoodDecrementSeconds = 5.0;
static double maxFoodDecrementSeconds = 20;
static double foodScaleFactor = 1.0;

static double indoorFoodDecrementSecondsBonus = 20.0;

static int babyBirthFoodDecrement = 10;

// bonus applied to all foods
// makes whole server a bit easier (or harder, if negative)
static int eatBonus = 0;

static double posseSizeSpeedMultipliers[4] = { 0.75, 1.25, 1.5, 2.0 };



static int minActivePlayersForLanguages = 15;


// keep a running sequence number to challenge each connecting client
// to produce new login hashes, avoiding replay attacks.
static unsigned int nextSequenceNumber = 1;


static int requireClientPassword = 1;
static int requireTicketServerCheck = 1;
static char *clientPassword = NULL;
static char *ticketServerURL = NULL;
static char *reflectorURL = NULL;

// larger of dataVersionNumber.txt or serverCodeVersionNumber.txt
static int versionNumber = 1;


static double childSameRaceLikelihood = 0.9;
static int familySpan = 2;


// phrases that trigger baby and family naming
static SimpleVector<char*> nameGivingPhrases;
static SimpleVector<char*> familyNameGivingPhrases;
static SimpleVector<char*> eveNameGivingPhrases;
static SimpleVector<char*> cursingPhrases;

char *curseYouPhrase = NULL;
char *curseBabyPhrase = NULL;

static SimpleVector<char*> youGivingPhrases;
static SimpleVector<char*> namedGivingPhrases;

static SimpleVector<char*> familyGivingPhrases;
static SimpleVector<char*> offspringGivingPhrases;

static SimpleVector<char*> posseJoiningPhrases;


static SimpleVector<char*> youFollowPhrases;
static SimpleVector<char*> namedFollowPhrases;

static SimpleVector<char*> youExilePhrases;
static SimpleVector<char*> namedExilePhrases;


static SimpleVector<char*> youRedeemPhrases;
static SimpleVector<char*> namedRedeemPhrases;


static SimpleVector<char*> youKillPhrases;
static SimpleVector<char*> namedKillPhrases;
static SimpleVector<char*> namedAfterKillPhrases;



static char *eveName = NULL;


// maps extended ascii codes to true/false for characters allowed in SAY
// messages
static char allowedSayCharMap[256];

static const char *allowedSayChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-,'?! ";


static int killEmotionIndex = 2;
static int victimEmotionIndex = 2;


static double lastBabyPassedThresholdTime = 0;


static double eveWindowStart = 0;
static char eveWindowOver = false;


typedef struct PeaceTreaty {
        int lineageAEveID;
        int lineageBEveID;
        
        // they have to say it in both directions
        // before it comes into effect
        char dirAToB;
        char dirBToA;

        // track directions of breaking it later
        char dirAToBBroken;
        char dirBToABroken;
    } PeaceTreaty;

    

static SimpleVector<PeaceTreaty> peaceTreaties;


typedef struct WarState {
        int lineageAEveID;
        int lineageBEveID;
    } WarState;

static SimpleVector<WarState> warStates;



// may be partial
static PeaceTreaty *getMatchingTreaty( int inLineageAEveID, 
                                       int inLineageBEveID ) {
    
    for( int i=0; i<peaceTreaties.size(); i++ ) {
        PeaceTreaty *p = peaceTreaties.getElement( i );
        

        if( ( p->lineageAEveID == inLineageAEveID &&
              p->lineageBEveID == inLineageBEveID )
            ||
            ( p->lineageAEveID == inLineageBEveID &&
              p->lineageBEveID == inLineageAEveID ) ) {
            // they match a treaty.
            return p;
            }
        }
    return NULL;
    }



// parial treaty returned if it's requested
static char isPeaceTreaty( int inLineageAEveID, int inLineageBEveID,
                           PeaceTreaty **outPartialTreaty = NULL ) {
    
    PeaceTreaty *p = getMatchingTreaty( inLineageAEveID, inLineageBEveID );
        
    if( p != NULL ) {
        
        if( !( p->dirAToB && p->dirBToA ) ) {
            // partial treaty
            if( outPartialTreaty != NULL ) {
                *outPartialTreaty = p;
                }
            return false;
            }
        return true;
        }
    return false;
    }



static char isWarState( int inLineageAEveID, int inLineageBEveID ) {
    for( int i=0; i<warStates.size(); i++ ) {
        WarState *w = warStates.getElement( i );
        
        
        if( ( w->lineageAEveID == inLineageAEveID &&
              w->lineageBEveID == inLineageBEveID )
            ||
            ( w->lineageAEveID == inLineageBEveID &&
              w->lineageBEveID == inLineageAEveID ) ) {
            
            return true;
            }
        }
    return false;
    }






void sendWarReportToAll();


void sendPeaceWarMessage( const char *inPeaceOrWar,
                          char inWar,
                          int inLineageAEveID, int inLineageBEveID );


static void addPeaceTreaty( int inLineageAEveID, int inLineageBEveID ) {
    PeaceTreaty *p = getMatchingTreaty( inLineageAEveID, inLineageBEveID );
    
    if( p != NULL ) {
        char peaceBefore = p->dirAToB && p->dirBToA;
        
        // maybe it has been sealed in a new direction?
        if( p->lineageAEveID == inLineageAEveID ) {
            p->dirAToB = true;
            p->dirBToABroken = false;
            }
        if( p->lineageBEveID == inLineageAEveID ) {
            p->dirBToA = true;
            p->dirBToABroken = false;
            }
        if( p->dirAToB && p->dirBToA &&
            ! peaceBefore ) {
            // new peace!

            // clear any war state
            for( int i=0; i<warStates.size(); i++ ) {
                WarState *w = warStates.getElement( i );
                
                
                if( ( w->lineageAEveID == inLineageAEveID &&
                      w->lineageBEveID == inLineageBEveID )
                    ||
                    ( w->lineageAEveID == inLineageBEveID &&
                      w->lineageBEveID == inLineageAEveID ) ) {
                    
                    warStates.deleteElement( i );
                    break;
                    }
                }

            sendPeaceWarMessage( "PEACE", 
                                 false,
                                 p->lineageAEveID, p->lineageBEveID );
            sendWarReportToAll();
            }
        }
    else {
        // else doesn't exist, create new unidirectional
        PeaceTreaty p = { inLineageAEveID, inLineageBEveID,
                          true, false,
                          false, false };
        
        peaceTreaties.push_back( p );
        }
    }



static void removePeaceTreaty( int inLineageAEveID, int inLineageBEveID ) {
    PeaceTreaty *p = getMatchingTreaty( inLineageAEveID, inLineageBEveID );
    
    char remove = false;
    
    char messageSent = false;
    
    if( p != NULL ) {
        if( p->dirAToB && p->dirBToA ) {
            // established
            
            // maybe it has been broken in a new direction?
            if( p->lineageAEveID == inLineageAEveID ) {
                p->dirAToBBroken = true;
                }
            if( p->lineageBEveID == inLineageAEveID ) {
                p->dirBToABroken = true;
                }
            
            if( p->dirAToBBroken && p->dirBToABroken ) {
                // fully broken
                // remove it
                remove = true;

                // new war!
                sendPeaceWarMessage( "WAR",
                                     true,
                                     p->lineageAEveID, p->lineageBEveID );
                messageSent = true;
                }
            }
        else {
            // not fully established
            // remove it 
            
            // this means if one person says PEACE and the other
            // responds with WAR, the first person's PEACE half-way treaty
            // is canceled.  Both need to say PEACE again once WAR has been
            // mentioned
            remove = true;
            }
        }

    
    if( remove || p == NULL ) {
        // no treaty exists, or it will be removed
        
        // some elder said "WAR"
        // war state created if it doesn't exist
        
        char found = false;
        for( int i=0; i<warStates.size(); i++ ) {
            WarState *w = warStates.getElement( i );
        

            if( ( w->lineageAEveID == inLineageAEveID &&
                  w->lineageBEveID == inLineageBEveID )
                ||
                ( w->lineageAEveID == inLineageBEveID &&
                  w->lineageBEveID == inLineageAEveID ) ) {
                found = true;
                break;
                }
            }
        
        if( !found ) {
            // add new one
            WarState w = { inLineageAEveID, inLineageBEveID };
            warStates.push_back( w );

            if( ! messageSent ) {
                sendPeaceWarMessage( "WAR", 
                                     true,
                                     inLineageAEveID, inLineageBEveID );
                messageSent = true;
                }
            }
        }
    
    if( messageSent ) {
        sendWarReportToAll();
        }

    if( remove ) {
        for( int i=0; i<peaceTreaties.size(); i++ ) {
            PeaceTreaty *otherP = peaceTreaties.getElement( i );
            
            if( otherP->lineageAEveID == p->lineageAEveID &&
                otherP->lineageBEveID == p->lineageBEveID ) {
                
                peaceTreaties.deleteElement( i );
                return;
                }
            }
        }
    }


typedef struct PastLifeStats {
        int lifeCount;
        int lifeTotalSeconds;
        char error;
    } PastLifeStats;

    



// for incoming socket connections that are still in the login process
typedef struct FreshConnection {
        Socket *sock;
        SimpleVector<char> *sockBuffer;

        unsigned int sequenceNumber;
        char *sequenceNumberString;
        
        WebRequest *ticketServerRequest;
        char ticketServerAccepted;
        char lifeTokenSpent;

        float fitnessScore;

        double ticketServerRequestStartTime;
        
        char error;
        const char *errorCauseString;
        
        double rejectedSendTime;

        char shutdownMode;

        // for tracking connections that have failed to LOGIN 
        // in a timely manner
        double connectionStartTimeSeconds;

        char *email;
        
        int tutorialNumber;
        CurseStatus curseStatus;
        PastLifeStats lifeStats;
        
        char *twinCode;
        int twinCount;

        char *clientTag;

    } FreshConnection;


SimpleVector<FreshConnection> newConnections;

SimpleVector<FreshConnection> waitingForTwinConnections;



typedef struct LiveObject {
        char *email;
        // for tracking old email after player has been deleted 
        // but is still on list
        char *origEmail;

        int id;
        
        float fitnessScore;
        
        int numToolSlots;
        // these aren't object IDs but tool set index numbers
        // some tools are grouped together
        SimpleVector<int> learnedTools;


        // object ID used to visually represent this player
        int displayID;
        
        char *name;
        char nameHasSuffix;
        
        char *familyName;
        

        char *lastSay;

        CurseStatus curseStatus;
        PastLifeStats lifeStats;
        
        int curseTokenCount;
        char curseTokenUpdate;


        char isEve;        

        char isTutorial;

        char isTwin;
        
        // used to track incremental tutorial map loading
        TutorialLoadProgress tutorialLoad;


        GridPos birthPos;
        GridPos originalBirthPos;
        

        int parentID;

        // 0 for Eve
        int parentChainLength;

        SimpleVector<int> *lineage;
        
        SimpleVector<int> *ancestorIDs;
        SimpleVector<char*> *ancestorEmails;
        SimpleVector<char*> *ancestorRelNames;
        SimpleVector<double> *ancestorLifeStartTimeSeconds;

        // id of Eve that started this line
        int lineageEveID;
        

        // who this player is following
        // might be a dead player
        // -1 means following self (no one)
        int followingID;
        
        // -1 if not set
        int leadingColorIndex;

        // people who have exiled this player
        // some could be dead
        SimpleVector<int> exiledByIDs;
        
        char followingUpdate;
        char exileUpdate;


        // time that this life started (for computing age)
        // not actual creation time (can be adjusted to tweak starting age,
        // for example, in case of Eve who starts older).
        double lifeStartTimeSeconds;

        // time when this player actually died
        double deathTimeSeconds;
        
        
        // the wall clock time when this life started
        // used for computing playtime, not age
        double trueStartTimeSeconds;
        

        double lastSayTimeSeconds;

        double firstEmoteTimeSeconds;
        int emoteCountInWindow;
        char emoteCooldown;
        double emoteCooldownStartTimeSeconds;


        // held by other player?
        char heldByOther;
        int heldByOtherID;
        char everHeldByParent;

        // player that's responsible for updates that happen to this
        // player during current step
        int responsiblePlayerID;

        // affects movement speed if part of posse
        int killPosseSize;
        

        // start and dest for a move
        // same if reached destination
        int xs;
        int ys;
        
        int xd;
        int yd;
        
        // next player update should be flagged
        // as a forced position change
        char posForced;
        
        char waitingForForceResponse;
        
        int lastMoveSequenceNumber;
        

        int pathLength;
        GridPos *pathToDest;
        
        char pathTruncated;

        char firstMapSent;
        int lastSentMapX;
        int lastSentMapY;
        
        // path dest for the last full path that we checked completely
        // for getting too close to player's known map chunk
        GridPos mapChunkPathCheckedDest;
        

        double moveTotalSeconds;
        double moveStartTime;
        
        double pathDist;
        

        int facingOverride;
        int actionAttempt;
        GridPos actionTarget;
        
        int holdingID;

        // absolute time in seconds that what we're holding should decay
        // or 0 if it never decays
        timeSec_t holdingEtaDecay;


        // where on map held object was picked up from
        char heldOriginValid;
        int heldOriginX;
        int heldOriginY;
        

        // track origin of held separate to use when placing a grave
        int heldGraveOriginX;
        int heldGraveOriginY;
        int heldGravePlayerID;
        

        // if held object was created by a transition on a target, what is the
        // object ID of the target from the transition?
        int heldTransitionSourceID;
        

        int numContained;
        int *containedIDs;
        timeSec_t *containedEtaDecays;

        // vector of sub-contained for each contained item
        SimpleVector<int> *subContainedIDs;
        SimpleVector<timeSec_t> *subContainedEtaDecays;
        

        // if they've been killed and part of a weapon (bullet?) has hit them
        // this will be included in their grave
        int embeddedWeaponID;
        timeSec_t embeddedWeaponEtaDecay;
        
        // and what original weapon killed them?
        int murderSourceID;
        char holdingWound;

        char holdingBiomeSickness;

        // who killed them?
        int murderPerpID;
        char *murderPerpEmail;
        
        // or if they were killed by a non-person, what was it?
        int deathSourceID;
        
        // true if this character landed a mortal wound on another player
        char everKilledAnyone;

        // true in case of sudden infant death
        char suicide;
        

        Socket *sock;
        SimpleVector<char> *sockBuffer;
        
        // indicates that some messages were sent to this player this 
        // frame, and they need a FRAME terminator message
        char gotPartOfThisFrame;
        

        char isNew;
        char firstMessageSent;
        
        char inFlight;
        

        char dying;
        // wall clock time when they will be dead
        double dyingETA;

        // in cases where their held wound produces a forced emot
        char emotFrozen;
        double emotUnfreezeETA;
        int emotFrozenIndex;
        
        char connected;
        
        char error;
        const char *errorCauseString;
        
        

        int customGraveID;
        
        char *deathReason;

        char deleteSent;
        // wall clock time when we consider the delete good and sent
        // and can close their connection
        double deleteSentDoneETA;

        char deathLogged;

        char newMove;
        
        // heat map that player carries around with them
        // every time they stop moving, it is updated to compute
        // their local temp
        float heatMap[ HEAT_MAP_D * HEAT_MAP_D ];

        // net heat of environment around player
        // map is tracked in heat units (each object produces an 
        // integer amount of heat)
        // this is in base heat units, range 0 to infinity
        float envHeat;

        // amount of heat currently in player's body, also in
        // base heat units
        float bodyHeat;
        

        // used track current biome heat for biome-change shock effects
        float biomeHeat;
        float lastBiomeHeat;


        // body heat normalized to [0,1], with targetHeat at 0.5
        float heat;
        
        // flags this player as needing to recieve a heat update
        char heatUpdate;
        
        // wall clock time of last time this player was sent
        // a heat update
        double lastHeatUpdate;

        // true if heat map features player surrounded by walls
        char isIndoors;
        
        double foodDrainTime;
        double indoorBonusTime;
        double indoorBonusFraction;
        

        int foodStore;
        
        double foodCapModifier;

        double drunkenness;


        double fever;
        

        // wall clock time when we should decrement the food store
        double foodDecrementETASeconds;
        
        // should we send player a food status message
        char foodUpdate;
        
        // info about the last thing we ate, for FX food messages sent
        // just to player
        int lastAteID;
        int lastAteFillMax;
        
        // this is for PU messages sent to everyone
        char justAte;
        int justAteID;
        
        // chain of non-repeating foods eaten
        SimpleVector<int> yummyFoodChain;
        
        // how many bonus from yummy food is stored
        // these are used first before food is decremented
        int yummyBonusStore;
        
        // last time we told player their capacity in a food update
        // what did we tell them?
        int lastReportedFoodCapacity;
        

        ClothingSet clothing;
        
        timeSec_t clothingEtaDecay[NUM_CLOTHING_PIECES];

        SimpleVector<int> clothingContained[NUM_CLOTHING_PIECES];
        
        SimpleVector<timeSec_t> 
            clothingContainedEtaDecays[NUM_CLOTHING_PIECES];

        char needsUpdate;
        char updateSent;
        char updateGlobal;
        
        char wiggleUpdate;
        

        // babies born to this player
        SimpleVector<timeSec_t> *babyBirthTimes;
        SimpleVector<int> *babyIDs;
        
        // wall clock time after which they can have another baby
        // starts at 0 (start of time epoch) for non-mothers, as
        // they can have their first baby right away.
        timeSec_t birthCoolDown;
        

        timeSec_t lastRegionLookTime;
        
        double playerCrossingCheckTime;
        

        char monumentPosSet;
        GridPos lastMonumentPos;
        int lastMonumentID;
        char monumentPosSent;
        

        char holdingFlightObject;
        
        char vogMode;
        GridPos preVogPos;
        GridPos preVogBirthPos;
        int vogJumpIndex;
        char postVogMode;
        
        char forceSpawn;
        

        // list of positions owned by this player
        SimpleVector<GridPos> ownedPositions;

        // list of owned positions that this player has heard about
        SimpleVector<GridPos> knownOwnedPositions;

        GridPos forceFlightDest;
        double forceFlightDestSetTime;
        
        SimpleVector<int> permanentEmots;

    } LiveObject;



SimpleVector<LiveObject> players;
SimpleVector<LiveObject> tutorialLoadingPlayers;



char doesEveLineExist( int inEveID ) {
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( ( ! o->error ) && o->lineageEveID == inEveID ) {
            return true;
            }
        }
    return false;
    }




typedef struct DeadObject {
        int id;
        
        int displayID;
        
        char *name;
        
        SimpleVector<int> *lineage;
        
        // id of Eve that started this line
        int lineageEveID;
        


        // time that this life started (for computing age)
        // not actual creation time (can be adjusted to tweak starting age,
        // for example, in case of Eve who starts older).
        double lifeStartTimeSeconds;
        
        // time this person died
        double deathTimeSeconds;
        

    } DeadObject;



static double lastPastPlayerFlushTime = 0;

SimpleVector<DeadObject> pastPlayers;



static void addPastPlayer( LiveObject *inPlayer ) {
    
    DeadObject o;
    
    o.id = inPlayer->id;
    o.displayID = inPlayer->displayID;
    o.name = NULL;
    if( inPlayer->name != NULL ) {
        o.name = stringDuplicate( inPlayer->name );
        }
    o.lineageEveID = inPlayer->lineageEveID;
    o.lifeStartTimeSeconds = inPlayer->lifeStartTimeSeconds;
    o.deathTimeSeconds = inPlayer->deathTimeSeconds;
    
    o.lineage = new SimpleVector<int>();
    for( int i=0; i< inPlayer->lineage->size(); i++ ) {
        o.lineage->push_back( inPlayer->lineage->getElementDirect( i ) );
        }
    
    pastPlayers.push_back( o );
    }



char isOwned( LiveObject *inPlayer, int inX, int inY ) {
    for( int i=0; i<inPlayer->ownedPositions.size(); i++ ) {
        GridPos *p = inPlayer->ownedPositions.getElement( i );
        
        if( p->x == inX && p->y == inY ) {
            return true;
            }
        }
    return false;
    }



char isOwned( LiveObject *inPlayer, GridPos inPos ) {
    return isOwned( inPlayer, inPos.x, inPos.y );
    }



char isKnownOwned( LiveObject *inPlayer, int inX, int inY ) {
    for( int i=0; i<inPlayer->knownOwnedPositions.size(); i++ ) {
        GridPos *p = inPlayer->knownOwnedPositions.getElement( i );
        
        if( p->x == inX && p->y == inY ) {
            return true;
            }
        }
    return false;
    }



char isKnownOwned( LiveObject *inPlayer, GridPos inPos ) {
    return isKnownOwned( inPlayer, inPos.x, inPos.y );
    }



SimpleVector<GridPos> recentlyRemovedOwnerPos;


void removeAllOwnership( LiveObject *inPlayer ) {
    double startTime = Time::getCurrentTime();
    int num = inPlayer->ownedPositions.size();
    
    for( int i=0; i<inPlayer->ownedPositions.size(); i++ ) {
        GridPos *p = inPlayer->ownedPositions.getElement( i );

        recentlyRemovedOwnerPos.push_back( *p );
        
        int oID = getMapObject( p->x, p->y );

        if( oID <= 0 ) {
            continue;
            }

        char noOtherOwners = true;
        
        for( int j=0; j<players.size(); j++ ) {
            LiveObject *otherPlayer = players.getElement( j );
            
            if( otherPlayer != inPlayer ) {
                if( isOwned( otherPlayer, *p ) ) {
                    noOtherOwners = false;
                    break;
                    }
                }
            }
        
        if( noOtherOwners ) {
            // last owner of p just died
            // force end transition
            SimpleVector<int> *deathMarkers = getAllPossibleDeathIDs();
            for( int j=0; j<deathMarkers->size(); j++ ) {
                int deathID = deathMarkers->getElementDirect( j );
                TransRecord *t = getTrans( deathID, oID );
                
                if( t != NULL ) {
                    
                    setMapObject( p->x, p->y, t->newTarget );
                    break;
                    }
                }
            }
        }
    
    inPlayer->ownedPositions.deleteAll();

    AppLog::infoF( "Removing all ownership (%d owned) for "
                   "player %d (%s) took %lf sec",
                   num, inPlayer->id, inPlayer->email, 
                   Time::getCurrentTime() - startTime );
    }



char *getOwnershipString( int inX, int inY ) {    
    SimpleVector<char> messageWorking;
    
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *otherPlayer = players.getElement( j );
        if( ! otherPlayer->error &&
            isOwned( otherPlayer, inX, inY ) ) {
            char *playerIDString = 
                autoSprintf( " %d", otherPlayer->id );
            messageWorking.appendElementString( 
                playerIDString );
            delete [] playerIDString;
            }
        }
    char *message = messageWorking.getElementString();
    return message;
    }


char *getOwnershipString( GridPos inPos ) {
    return getOwnershipString( inPos.x, inPos.y );
    }



static char checkReadOnly() {
    const char *testFileName = "testReadOnly.txt";
    
    FILE *testFile = fopen( testFileName, "w" );
    
    if( testFile != NULL ) {
        
        fclose( testFile );
        remove( testFileName );
        return false;
        }
    return true;
    }




// returns a person to their natural state
static void backToBasics( LiveObject *inPlayer ) {
    LiveObject *p = inPlayer;

    // do not heal dying people
    if( ! p->holdingWound && p->holdingID > 0 ) {
        
        p->holdingID = 0;
        
        p->holdingEtaDecay = 0;
        
        p->heldOriginValid = false;
        p->heldTransitionSourceID = -1;
        
        
        p->numContained = 0;
        if( p->containedIDs != NULL ) {
            delete [] p->containedIDs;
            delete [] p->containedEtaDecays;
            p->containedIDs = NULL;
        p->containedEtaDecays = NULL;
            }
        
        if( p->subContainedIDs != NULL ) {
            delete [] p->subContainedIDs;
            delete [] p->subContainedEtaDecays;
            p->subContainedIDs = NULL;
            p->subContainedEtaDecays = NULL;
            }
        }
        
        
    p->clothing = getEmptyClothingSet();
    
    for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
        p->clothingEtaDecay[c] = 0;
        p->clothingContained[c].deleteAll();
        p->clothingContainedEtaDecays[c].deleteAll();
        }

    p->emotFrozen = false;
    p->emotUnfreezeETA = 0;

    p->learnedTools.deleteAll();
    }




typedef struct GraveInfo {
        GridPos pos;
        int playerID;
        // eve that started the line of this dead person
        // used for tracking whether grave is part of player's family or not
        int lineageEveID;
    } GraveInfo;


typedef struct GraveMoveInfo {
        GridPos posStart;
        GridPos posEnd;
        int swapDest;
    } GraveMoveInfo;




// tracking spots on map that inflicted a mortal wound
// put them on timeout afterward so that they don't attack
// again immediately
typedef struct DeadlyMapSpot {
        GridPos pos;
        double timeOfAttack;
    } DeadlyMapSpot;


static double deadlyMapSpotTimeoutSec = 10;

static SimpleVector<DeadlyMapSpot> deadlyMapSpots;


static char wasRecentlyDeadly( GridPos inPos ) {
    double curTime = Time::getCurrentTime();
    
    for( int i=0; i<deadlyMapSpots.size(); i++ ) {
        
        DeadlyMapSpot *s = deadlyMapSpots.getElement( i );
        
        if( curTime - s->timeOfAttack >= deadlyMapSpotTimeoutSec ) {
            deadlyMapSpots.deleteElement( i );
            i--;
            }
        else if( s->pos.x == inPos.x && s->pos.y == inPos.y ) {
            // note that this is a lazy method that only walks through
            // the whole list and checks for timeouts when
            // inPos isn't found
            return true;
            }
        }
    return false;
    }



static void addDeadlyMapSpot( GridPos inPos ) {
    // don't check for duplicates
    // we're only called to add a new deadly spot when the spot isn't
    // currently on deadly cooldown anyway
    DeadlyMapSpot s = { inPos, Time::getCurrentTime() };
    deadlyMapSpots.push_back( s );
    }




static LiveObject *getLiveObject( int inID ) {
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( o->id == inID ) {
            return o;
            }
        }
    
    return NULL;
    }


char *getPlayerName( int inID ) {
    LiveObject *o = getLiveObject( inID );
    if( o != NULL ) {
        return o->name;
        }
    return NULL;
    }




static double pickBirthCooldownSeconds() {
    // Kumaraswamy distribution
    // PDF:
    // k(x,a,b) = a * b * x**( a - 1 ) * (1-x**a)**(b-1)
    // CDF:
    // kCDF(x,a,b) = 1 - (1-x**a)**b
    // Invers CDF:
    // kCDFInv(y,a,b) = ( 1 - (1-y)**(1.0/b) )**(1.0/a)

    // For b=1, PDF curve starts at 0 and curves upward, for all a > 2
    // good values seem to be a=1.5, b=1

    // actually, make it more bell-curve like, with a=2, b=3

    double a = 2;
    double b = 3;
    
    // mean is around 2 minutes
    

    // uniform
    double u = randSource.getRandomDouble();
    
    // feed into inverted CDF to sample a value from the distribution
    double v = pow( 1 - pow( 1-u, (1/b) ), 1/a );
    
    // v is in [0..1], the value range of Kumaraswamy

    // put max at 5 minutes
    return v * 5 * 60;
    }




typedef struct FullMapContained{ 
        int numContained;
        int *containedIDs;
        timeSec_t *containedEtaDecays;
        SimpleVector<int> *subContainedIDs;
        SimpleVector<timeSec_t> *subContainedEtaDecays;
    } FullMapContained;



// including contained and sub contained in one call
FullMapContained getFullMapContained( int inX, int inY ) {
    FullMapContained r;
    
    r.containedIDs = getContained( inX, inY, &( r.numContained ) );
    r.containedEtaDecays = 
        getContainedEtaDecay( inX, inY, &( r.numContained ) );
    
    if( r.numContained == 0 ) {
        r.subContainedIDs = NULL;
        r.subContainedEtaDecays = NULL;
        }
    else {
        r.subContainedIDs = new SimpleVector<int>[ r.numContained ];
        r.subContainedEtaDecays = new SimpleVector<timeSec_t>[ r.numContained ];
        }
    
    for( int c=0; c< r.numContained; c++ ) {
        if( r.containedIDs[c] < 0 ) {
            
            int numSub;
            int *subContainedIDs = getContained( inX, inY, &numSub,
                                                 c + 1 );
            
            if( subContainedIDs != NULL ) {
                
                r.subContainedIDs[c].appendArray( subContainedIDs, numSub );
                delete [] subContainedIDs;
                }
            
            timeSec_t *subContainedEtaDecays = 
                getContainedEtaDecay( inX, inY, &numSub,
                                      c + 1 );

            if( subContainedEtaDecays != NULL ) {
                
                r.subContainedEtaDecays[c].appendArray( subContainedEtaDecays, 
                                                        numSub );
                delete [] subContainedEtaDecays;
                }
            }
        }
    
    return r;
    }



void freePlayerContainedArrays( LiveObject *inPlayer ) {
    if( inPlayer->containedIDs != NULL ) {
        delete [] inPlayer->containedIDs;
        }
    if( inPlayer->containedEtaDecays != NULL ) {
        delete [] inPlayer->containedEtaDecays;
        }
    if( inPlayer->subContainedIDs != NULL ) {
        delete [] inPlayer->subContainedIDs;
        }
    if( inPlayer->subContainedEtaDecays != NULL ) {
        delete [] inPlayer->subContainedEtaDecays;
        }

    inPlayer->containedIDs = NULL;
    inPlayer->containedEtaDecays = NULL;
    inPlayer->subContainedIDs = NULL;
    inPlayer->subContainedEtaDecays = NULL;
    }



void setContained( LiveObject *inPlayer, FullMapContained inContained ) {
    
    inPlayer->numContained = inContained.numContained;
     
    freePlayerContainedArrays( inPlayer );
    
    inPlayer->containedIDs = inContained.containedIDs;
    
    inPlayer->containedEtaDecays =
        inContained.containedEtaDecays;
    
    inPlayer->subContainedIDs =
        inContained.subContainedIDs;
    inPlayer->subContainedEtaDecays =
        inContained.subContainedEtaDecays;
    }
    
    
    
    
void clearPlayerHeldContained( LiveObject *inPlayer ) {
    inPlayer->numContained = 0;
    
    delete [] inPlayer->containedIDs;
    delete [] inPlayer->containedEtaDecays;
    delete [] inPlayer->subContainedIDs;
    delete [] inPlayer->subContainedEtaDecays;
    
    inPlayer->containedIDs = NULL;
    inPlayer->containedEtaDecays = NULL;
    inPlayer->subContainedIDs = NULL;
    inPlayer->subContainedEtaDecays = NULL;
    }
    



void transferHeldContainedToMap( LiveObject *inPlayer, int inX, int inY ) {
    if( inPlayer->numContained != 0 ) {
        timeSec_t curTime = Time::timeSec();
        float stretch = 
            getObject( inPlayer->holdingID )->slotTimeStretch;
        
        for( int c=0;
             c < inPlayer->numContained;
             c++ ) {
            
            // undo decay stretch before adding
            // (stretch applied by adding)
            if( stretch != 1.0 &&
                inPlayer->containedEtaDecays[c] != 0 ) {
                
                timeSec_t offset = 
                    inPlayer->containedEtaDecays[c] - curTime;
                
                offset = offset * stretch;
                
                inPlayer->containedEtaDecays[c] =
                    curTime + offset;
                }

            addContained( 
                inX, inY,
                inPlayer->containedIDs[c],
                inPlayer->containedEtaDecays[c] );

            int numSub = inPlayer->subContainedIDs[c].size();
            if( numSub > 0 ) {

                int container = inPlayer->containedIDs[c];
                
                if( container < 0 ) {
                    container *= -1;
                    }
                
                float subStretch = getObject( container )->slotTimeStretch;
                    
                
                int *subIDs = 
                    inPlayer->subContainedIDs[c].getElementArray();
                timeSec_t *subDecays = 
                    inPlayer->subContainedEtaDecays[c].
                    getElementArray();
                
                for( int s=0; s < numSub; s++ ) {
                    
                    // undo decay stretch before adding
                    // (stretch applied by adding)
                    if( subStretch != 1.0 &&
                        subDecays[s] != 0 ) {
                
                        timeSec_t offset = subDecays[s] - curTime;
                        
                        offset = offset * subStretch;
                        
                        subDecays[s] = curTime + offset;
                        }

                    addContained( inX, inY,
                                  subIDs[s], subDecays[s],
                                  c + 1 );
                    }
                delete [] subIDs;
                delete [] subDecays;
                }
            }

        clearPlayerHeldContained( inPlayer );
        }
    }





// diags are square root of 2 in length
static double diagLength = 1.41421356237;
    


// diagonal steps are longer
static double measurePathLength( int inXS, int inYS, 
                                 GridPos *inPathPos, int inPathLength ) {

    double totalLength = 0;
    
    GridPos lastPos = { inXS, inYS };
    
    for( int i=0; i<inPathLength; i++ ) {
        
        GridPos thisPos = inPathPos[i];
        
        if( thisPos.x != lastPos.x &&
            thisPos.y != lastPos.y ) {
            totalLength += diagLength;
            }
        else {
            // not diag
            totalLength += 1;
            }
        lastPos = thisPos;
        }
    
    return totalLength;
    }




static double getPathSpeedModifier( GridPos *inPathPos, int inPathLength ) {
    
    if( inPathLength < 1 ) {
        return 1;
        }
    

    int floor = getMapFloor( inPathPos[0].x, inPathPos[0].y );

    if( floor == 0 ) {
        return 1;
        }

    double speedMult = getObject( floor )->speedMult;
    
    if( speedMult == 1 ) {
        return 1;
        }
    

    // else we have a speed mult for at least first step in path
    // see if we have same floor for rest of path

    for( int i=1; i<inPathLength; i++ ) {
        
        int thisFloor = getMapFloor( inPathPos[i].x, inPathPos[i].y );
        
        if( thisFloor != floor ) {
            // not same floor whole way
            return 1;
            }
        }
    // same floor whole way
    printf( "Speed modifier = %f\n", speedMult );
    return speedMult;
    }



static int getLiveObjectIndex( int inID ) {
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( o->id == inID ) {
            return i;
            }
        }

    return -1;
    }





int nextID = 2;


static void deleteMembers( FreshConnection *inConnection ) {
    delete inConnection->sock;
    delete inConnection->sockBuffer;
    
    if( inConnection->sequenceNumberString != NULL ) {    
        delete [] inConnection->sequenceNumberString;
        }
    
    if( inConnection->ticketServerRequest != NULL ) {
        delete inConnection->ticketServerRequest;
        }
    
    if( inConnection->email != NULL ) {
        delete [] inConnection->email;
        }
    
    if( inConnection->twinCode != NULL ) {
        delete [] inConnection->twinCode;
        }

    if( inConnection->clientTag != NULL ) {
        delete [] inConnection->clientTag;
        }
    }



static SimpleVector<char *> familyNamesAfterEveWindow;
static SimpleVector<int> familyLineageEveIDsAfterEveWindow;
static SimpleVector<int> familyCountsAfterEveWindow;

static int nextBabyFamilyIndex = 0;


static FILE *postWindowFamilyLogFile = NULL;




void quitCleanup() {
    AppLog::info( "Cleaning up on quit..." );

    // FreshConnections are in two different lists
    // free structures from both
    SimpleVector<FreshConnection> *connectionLists[2] =
        { &newConnections, &waitingForTwinConnections };

    for( int c=0; c<2; c++ ) {
        SimpleVector<FreshConnection> *list = connectionLists[c];
        
        for( int i=0; i<list->size(); i++ ) {
            FreshConnection *nextConnection = list->getElement( i );
            deleteMembers( nextConnection );
            }
        list->deleteAll();
        }
    
    // add these to players to clean them up togeter
    for( int i=0; i<tutorialLoadingPlayers.size(); i++ ) {
        LiveObject nextPlayer = tutorialLoadingPlayers.getElementDirect( i );
        players.push_back( nextPlayer );
        }
    tutorialLoadingPlayers.deleteAll();
    


    for( int i=0; i<players.size(); i++ ) {
        LiveObject *nextPlayer = players.getElement(i);
        
        removeAllOwnership( nextPlayer );

        if( nextPlayer->sock != NULL ) {
            delete nextPlayer->sock;
            nextPlayer->sock = NULL;
            }
        if( nextPlayer->sockBuffer != NULL ) {
            delete nextPlayer->sockBuffer;
            nextPlayer->sockBuffer = NULL;
            }

        delete nextPlayer->lineage;

        delete nextPlayer->ancestorIDs;

        nextPlayer->ancestorEmails->deallocateStringElements();
        delete nextPlayer->ancestorEmails;
        
        nextPlayer->ancestorRelNames->deallocateStringElements();
        delete nextPlayer->ancestorRelNames;
        
        delete nextPlayer->ancestorLifeStartTimeSeconds;
        

        if( nextPlayer->name != NULL ) {
            delete [] nextPlayer->name;
            }

        if( nextPlayer->familyName != NULL ) {
            delete [] nextPlayer->familyName;
            }

        if( nextPlayer->lastSay != NULL ) {
            delete [] nextPlayer->lastSay;
            }
        
        if( nextPlayer->email != NULL  ) {
            delete [] nextPlayer->email;
            }
        if( nextPlayer->origEmail != NULL  ) {
            delete [] nextPlayer->origEmail;
            }

        if( nextPlayer->murderPerpEmail != NULL  ) {
            delete [] nextPlayer->murderPerpEmail;
            }


        freePlayerContainedArrays( nextPlayer );
        
        
        if( nextPlayer->pathToDest != NULL ) {
            delete [] nextPlayer->pathToDest;
            }
        
        if( nextPlayer->deathReason != NULL ) {
            delete [] nextPlayer->deathReason;
            }


        delete nextPlayer->babyBirthTimes;
        delete nextPlayer->babyIDs;        
        }
    players.deleteAll();


    for( int i=0; i<pastPlayers.size(); i++ ) {
        DeadObject *o = pastPlayers.getElement( i );
        
        delete [] o->name;
        delete o->lineage;
        }
    pastPlayers.deleteAll();
    

    freeLineageLimit();
    
    freePlayerStats();
    freeLineageLog();
    
    freeNames();
    
    freeCurses();
    
    freeCurseDB();

    freeLifeTokens();

    freeFitnessScore();

    freeLifeLog();
    
    freeFoodLog();
    freeFailureLog();
    
    freeObjectSurvey();
    
    freeLanguage();
    freeFamilySkipList();

    freeTriggers();

    freeSpecialBiomes();
    

    freeMap();

    freeTransBank();
    freeCategoryBank();
    freeObjectBank();
    freeAnimationBank();
    
    freeArcReport();
    

    if( clientPassword != NULL ) {
        delete [] clientPassword;
        clientPassword = NULL;
        }
    

    if( ticketServerURL != NULL ) {
        delete [] ticketServerURL;
        ticketServerURL = NULL;
        }

    if( reflectorURL != NULL ) {
        delete [] reflectorURL;
        reflectorURL = NULL;
        }

    nameGivingPhrases.deallocateStringElements();
    familyNameGivingPhrases.deallocateStringElements();
    eveNameGivingPhrases.deallocateStringElements();
    cursingPhrases.deallocateStringElements();
    youGivingPhrases.deallocateStringElements();
    namedGivingPhrases.deallocateStringElements();
    
    familyGivingPhrases.deallocateStringElements();
    offspringGivingPhrases.deallocateStringElements();
    
    posseJoiningPhrases.deallocateStringElements();
    
    youFollowPhrases.deallocateStringElements();
    namedFollowPhrases.deallocateStringElements();
    
    youExilePhrases.deallocateStringElements();
    namedExilePhrases.deallocateStringElements();

    youRedeemPhrases.deallocateStringElements();
    namedRedeemPhrases.deallocateStringElements();


    youKillPhrases.deallocateStringElements();
    namedKillPhrases.deallocateStringElements();
    namedAfterKillPhrases.deallocateStringElements();
    


    if( curseYouPhrase != NULL ) {
        delete [] curseYouPhrase;
        curseYouPhrase = NULL;
        }
    if( curseBabyPhrase != NULL ) {
        delete [] curseBabyPhrase;
        curseBabyPhrase = NULL;
        }
    

    if( eveName != NULL ) {
        delete [] eveName;
        eveName = NULL;
        }

    if( apocalypseRequest != NULL ) {
        delete apocalypseRequest;
        apocalypseRequest = NULL;
        }

    if( familyDataLogFile != NULL ) {
        fclose( familyDataLogFile );
        familyDataLogFile = NULL;
        }

    familyNamesAfterEveWindow.deallocateStringElements();
    familyLineageEveIDsAfterEveWindow.deleteAll();
    familyCountsAfterEveWindow.deleteAll();
    nextBabyFamilyIndex = 0;
    
    if( postWindowFamilyLogFile != NULL ) {
        fclose( postWindowFamilyLogFile );
        postWindowFamilyLogFile = NULL;
        }
    }






volatile char quit = false;

void intHandler( int inUnused ) {
    AppLog::info( "Quit received for unix" );
    
    // since we handled this singal, we will return to normal execution
    quit = true;
    }


#ifdef WIN_32
#include <windows.h>
BOOL WINAPI ctrlHandler( DWORD dwCtrlType ) {
    if( CTRL_C_EVENT == dwCtrlType ) {
        AppLog::info( "Quit received for windows" );
        
        // will auto-quit as soon as we return from this handler
        // so cleanup now
        //quitCleanup();
        
        // seems to handle CTRL-C properly if launched by double-click
        // or batch file
        // (just not if launched directly from command line)
        quit = true;
        }
    return true;
    }
#endif


int numConnections = 0;







// reads all waiting data from socket and stores it in buffer
// returns true if socket still good, false on error
char readSocketFull( Socket *inSock, SimpleVector<char> *inBuffer ) {

    char buffer[512];
    
    int numRead = inSock->receive( (unsigned char*)buffer, 512, 0 );
    
    if( numRead == -1 ) {

        if( ! inSock->isSocketInFDRange() ) {
            // the internal FD of this socket is out of range
            // probably some kind of heap corruption.

            // save a bug report
            int allow = 
                SettingsManager::getIntSetting( "allowBugReports", 0 );

            if( allow ) {
                char *bugName = 
                    autoSprintf( "bug_socket_%f", Time::getCurrentTime() );
                
                char *bugOutName = autoSprintf( "%s_out.txt", bugName );
                
                File outFile( NULL, "serverOut.txt" );
                if( outFile.exists() ) {
                    fflush( stdout );
                    File outCopyFile( NULL, bugOutName );
                    
                    outFile.copy( &outCopyFile );
                    }
                delete [] bugName;
                delete [] bugOutName;
                }
            }
        
            
        return false;
        }
    
    while( numRead > 0 ) {
        inBuffer->appendArray( buffer, numRead );

        numRead = inSock->receive( (unsigned char*)buffer, 512, 0 );
        }

    return true;
    }



// NULL if there's no full message available
char *getNextClientMessage( SimpleVector<char> *inBuffer ) {
    // find first terminal character #

    int index = inBuffer->getElementIndex( '#' );
        
    if( index == -1 ) {

        if( inBuffer->size() > 200 ) {
            // 200 characters with no message terminator?
            // client is sending us nonsense
            // cut it off here to avoid buffer overflow
            
            AppLog::info( "More than 200 characters in client receive buffer "
                          "with no messsage terminator present, "
                          "generating NONSENSE message." );
            
            return stringDuplicate( "NONSENSE 0 0" );
            }

        return NULL;
        }
    
    if( index > 1 && 
        inBuffer->getElementDirect( 0 ) == 'K' &&
        inBuffer->getElementDirect( 1 ) == 'A' ) {
        
        // a KA (keep alive) message
        // short-cicuit the processing here
        
        inBuffer->deleteStartElements( index + 1 );
        return NULL;
        }
    
        

    char *message = new char[ index + 1 ];
    
    // all but terminal character
    for( int i=0; i<index; i++ ) {
        message[i] = inBuffer->getElementDirect( i );
        }
    
    // delete from buffer, including terminal character
    inBuffer->deleteStartElements( index + 1 );
    
    message[ index ] = '\0';
    
    return message;
    }





typedef enum messageType {
	MOVE,
    USE,
    SELF,
    BABY,
    UBABY,
    REMV,
    SREMV,
    DROP,
    KILL,
    SAY,
    EMOT,
    JUMP,
    DIE,
    GRAVE,
    OWNER,
    FORCE,
    MAP,
    TRIGGER,
    BUG,
    PING,
    VOGS,
    VOGN,
    VOGP,
    VOGM,
    VOGI,
    VOGT,
    VOGX,
    PHOTO,
    UNKNOWN
    } messageType;




typedef struct ClientMessage {
        messageType type;
        int x, y, c, i, id;
        
        int trigger;
        int bug;

        // some messages have extra positions attached
        int numExtraPos;

        // NULL if there are no extra
        GridPos *extraPos;

        // null if type not SAY
        char *saidText;
        
        // null if type not BUG
        char *bugText;

        // for MOVE messages
        int sequenceNumber;

    } ClientMessage;


static int pathDeltaMax = 16;


// if extraPos present in result, destroyed by caller
// inMessage may be modified by this call
ClientMessage parseMessage( LiveObject *inPlayer, char *inMessage ) {
    
    char nameBuffer[100];
    
    ClientMessage m;
    
    m.i = -1;
    m.c = -1;
    m.id = -1;
    m.trigger = -1;
    m.numExtraPos = 0;
    m.extraPos = NULL;
    m.saidText = NULL;
    m.bugText = NULL;
    m.sequenceNumber = -1;
    
    // don't require # terminator here
    
    
    //int numRead = sscanf( inMessage, 
    //                      "%99s %d %d", nameBuffer, &( m.x ), &( m.y ) );
    

    // profiler finds sscanf as a hotspot
    // try a custom bit of code instead
    
    int numRead = 0;
    
    int parseLen = strlen( inMessage );
    if( parseLen > 99 ) {
        parseLen = 99;
        }
    
    for( int i=0; i<parseLen; i++ ) {
        if( inMessage[i] == ' ' ) {
            switch( numRead ) {
                case 0:
                    if( i != 0 ) {
                        memcpy( nameBuffer, inMessage, i );
                        nameBuffer[i] = '\0';
                        numRead++;
                        // rewind back to read the space again
                        // before the first number
                        i--;
                        }
                    break;
                case 1:
                    m.x = atoi( &( inMessage[i] ) );
                    numRead++;
                    break;
                case 2:
                    m.y = atoi( &( inMessage[i] ) );
                    numRead++;
                    break;
                }
            if( numRead == 3 ) {
                break;
                }
            }
        }
    

    
    if( numRead >= 2 &&
        strcmp( nameBuffer, "BUG" ) == 0 ) {
        m.type = BUG;
        m.bug = m.x;
        m.bugText = stringDuplicate( inMessage );
        return m;
        }


    if( numRead != 3 ) {
        
        if( numRead == 2 &&
            strcmp( nameBuffer, "TRIGGER" ) == 0 ) {
            m.type = TRIGGER;
            m.trigger = m.x;
            }
        else {
            m.type = UNKNOWN;
            }
        
        return m;
        }
    

    if( strcmp( nameBuffer, "MOVE" ) == 0) {
        m.type = MOVE;
        
        char *atPos = strstr( inMessage, "@" );
        
        int offset = 3;
        
        if( atPos != NULL ) {
            offset = 4;            
            }
        

        // in place, so we don't need to deallocate them
        SimpleVector<char *> *tokens =
            tokenizeStringInPlace( inMessage );
        
        // require an even number of extra coords beyond offset
        if( tokens->size() < offset + 2 || 
            ( tokens->size() - offset ) %2 != 0 ) {
            
            delete tokens;
            
            m.type = UNKNOWN;
            return m;
            }
        
        if( atPos != NULL ) {
            // skip @ symbol in token and parse int
            m.sequenceNumber = atoi( &( tokens->getElementDirect( 3 )[1] ) );
            }

        int numTokens = tokens->size();
        
        m.numExtraPos = (numTokens - offset) / 2;
        
        m.extraPos = new GridPos[ m.numExtraPos ];

        for( int e=0; e<m.numExtraPos; e++ ) {
            
            char *xToken = tokens->getElementDirect( offset + e * 2 );
            char *yToken = tokens->getElementDirect( offset + e * 2 + 1 );
            
            // profiler found sscanf is a bottleneck here
            // try atoi instead
            //sscanf( xToken, "%d", &( m.extraPos[e].x ) );
            //sscanf( yToken, "%d", &( m.extraPos[e].y ) );

            m.extraPos[e].x = atoi( xToken );
            m.extraPos[e].y = atoi( yToken );
            
            
            if( abs( m.extraPos[e].x ) > pathDeltaMax ||
                abs( m.extraPos[e].y ) > pathDeltaMax ) {
                // path goes too far afield
                
                // terminate it here
                m.numExtraPos = e;
                
                if( e == 0 ) {
                    delete [] m.extraPos;
                    m.extraPos = NULL;
                    }
                break;
                }
                

            // make them absolute
            m.extraPos[e].x += m.x;
            m.extraPos[e].y += m.y;
            }
        
        delete tokens;
        }
    else if( strcmp( nameBuffer, "JUMP" ) == 0 ) {
        m.type = JUMP;
        }
    else if( strcmp( nameBuffer, "DIE" ) == 0 ) {
        m.type = DIE;
        }
    else if( strcmp( nameBuffer, "GRAVE" ) == 0 ) {
        m.type = GRAVE;
        }
    else if( strcmp( nameBuffer, "OWNER" ) == 0 ) {
        m.type = OWNER;
        }
    else if( strcmp( nameBuffer, "FORCE" ) == 0 ) {
        m.type = FORCE;
        }
    else if( strcmp( nameBuffer, "USE" ) == 0 ) {
        m.type = USE;
        // read optional id parameter
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = -1;
            }
        }
    else if( strcmp( nameBuffer, "SELF" ) == 0 ) {
        m.type = SELF;

        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.i ) );
        
        if( numRead != 4 ) {
            m.type = UNKNOWN;
            }
        }
    else if( strcmp( nameBuffer, "UBABY" ) == 0 ) {
        m.type = UBABY;

        // id param optional
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.i ), &( m.id ) );
        
        if( numRead != 4 && numRead != 5 ) {
            m.type = UNKNOWN;
            }
        if( numRead != 5 ) {
            m.id = -1;
            }
        }
    else if( strcmp( nameBuffer, "BABY" ) == 0 ) {
        m.type = BABY;
        // read optional id parameter
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = -1;
            }
        }
    else if( strcmp( nameBuffer, "PING" ) == 0 ) {
        m.type = PING;
        // read unique id parameter
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = 0;
            }
        }
    else if( strcmp( nameBuffer, "SREMV" ) == 0 ) {
        m.type = SREMV;
        
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.c ),
                          &( m.i ) );
        
        if( numRead != 5 ) {
            m.type = UNKNOWN;
            }
        }
    else if( strcmp( nameBuffer, "REMV" ) == 0 ) {
        m.type = REMV;
        
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.i ) );
        
        if( numRead != 4 ) {
            m.type = UNKNOWN;
            }
        }
    else if( strcmp( nameBuffer, "DROP" ) == 0 ) {
        m.type = DROP;
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.c ) );
        
        if( numRead != 4 ) {
            m.type = UNKNOWN;
            }
        }
    else if( strcmp( nameBuffer, "KILL" ) == 0 ) {
        m.type = KILL;
        
        // read optional id parameter
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = -1;
            }
        }
    else if( strcmp( nameBuffer, "MAP" ) == 0 ) {
        m.type = MAP;
        }
    else if( strcmp( nameBuffer, "SAY" ) == 0 ) {
        m.type = SAY;

        // look after second space
        char *firstSpace = strstr( inMessage, " " );
        
        if( firstSpace != NULL ) {
            
            char *secondSpace = strstr( &( firstSpace[1] ), " " );
            
            if( secondSpace != NULL ) {

                char *thirdSpace = strstr( &( secondSpace[1] ), " " );
                
                if( thirdSpace != NULL ) {
                    m.saidText = stringDuplicate( &( thirdSpace[1] ) );
                    }
                }
            }
        }
    else if( strcmp( nameBuffer, "EMOT" ) == 0 ) {
        m.type = EMOT;

        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.i ) );
        
        if( numRead != 4 ) {
            m.type = UNKNOWN;
            }
        }
    else if( strcmp( nameBuffer, "VOGS" ) == 0 ) {
        m.type = VOGS;
        }
    else if( strcmp( nameBuffer, "VOGN" ) == 0 ) {
        m.type = VOGN;
        }
    else if( strcmp( nameBuffer, "VOGP" ) == 0 ) {
        m.type = VOGP;
        }
    else if( strcmp( nameBuffer, "VOGM" ) == 0 ) {
        m.type = VOGM;
        }
    else if( strcmp( nameBuffer, "VOGI" ) == 0 ) {
        m.type = VOGI;
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = -1;
            }
        }
    else if( strcmp( nameBuffer, "VOGT" ) == 0 ) {
        m.type = VOGT;

        // look after second space
        char *firstSpace = strstr( inMessage, " " );
        
        if( firstSpace != NULL ) {
            
            char *secondSpace = strstr( &( firstSpace[1] ), " " );
            
            if( secondSpace != NULL ) {

                char *thirdSpace = strstr( &( secondSpace[1] ), " " );
                
                if( thirdSpace != NULL ) {
                    m.saidText = stringDuplicate( &( thirdSpace[1] ) );
                    }
                }
            }
        }
    else if( strcmp( nameBuffer, "VOGX" ) == 0 ) {
        m.type = VOGX;
        }
   else if( strcmp( nameBuffer, "PHOTO" ) == 0 ) {
        m.type = PHOTO;
        numRead = sscanf( inMessage, 
                          "%99s %d %d %d", 
                          nameBuffer, &( m.x ), &( m.y ), &( m.id ) );
        
        if( numRead != 4 ) {
            m.id = 0;
            }
        }
     else {
        m.type = UNKNOWN;
        }
    
    // incoming client messages are relative to birth pos
    // except NOT map pull messages, which are absolute
    if( m.type != MAP ) {    
        m.x += inPlayer->birthPos.x;
        m.y += inPlayer->birthPos.y;

        for( int i=0; i<m.numExtraPos; i++ ) {
            m.extraPos[i].x += inPlayer->birthPos.x;
            m.extraPos[i].y += inPlayer->birthPos.y;
            }
        }

    return m;
    }



// computes a fractional index along path
// 1.25 means 1/4 way between index 1 and 2 on path
// thus, this can be as low as -1 (for starting position)
double computePartialMovePathStepPrecise( LiveObject *inPlayer ) {
    
    if( inPlayer->pathLength == 0 || inPlayer->pathToDest == NULL ) {
        return -1;
        }

    double fractionDone = 
        ( Time::getCurrentTime() - 
          inPlayer->moveStartTime )
        / inPlayer->moveTotalSeconds;
    
    if( fractionDone > 1 ) {
        fractionDone = 1;
        }
    
    if( fractionDone < 0 ) {
        fractionDone = 0;
        }

    if( fractionDone == 1 ) {
        // at last spot in path, no partial measurment necessary
        return inPlayer->pathLength - 1;
        }
    
    if( fractionDone == 0 ) {
        // at start location, before first spot in path
        return -1;
        }

    double distDone = fractionDone * inPlayer->pathDist;

    
    // walk through path steps until we see dist done
    double totalLength = 0;
    
    GridPos lastPos = { inPlayer->xs, inPlayer->ys };
    
    double lastPosDist = 0;

    for( int i=0; i<inPlayer->pathLength; i++ ) {

        GridPos thisPos = inPlayer->pathToDest[i];
        
        double stepLen;
        

        if( thisPos.x != lastPos.x &&
            thisPos.y != lastPos.y ) {
            stepLen = diagLength;
            }
        else {
            // not diag
            stepLen = 1;
            }

        totalLength += stepLen;

        if( totalLength > distDone ) {
            // add in extra
            return ( i - 1 ) + (distDone - lastPosDist) / stepLen;
            }

        lastPos = thisPos;
        lastPosDist += stepLen;
        }
    
    return inPlayer->pathLength - 1;
    }




int computePartialMovePathStep( LiveObject *inPlayer ) {
    return lrint( computePartialMovePathStepPrecise( inPlayer ) );
    }



doublePair computePartialMoveSpotPrecise( LiveObject *inPlayer ) {

    double c = computePartialMovePathStepPrecise( inPlayer );
    
    if( c == -1 ) {
        doublePair result = { (double)inPlayer->xs, 
                              (double)inPlayer->ys };
        return result;
        }

    
    int aInd = floor( c );
    int bInd = ceil( c );
    
    
    GridPos aPos;
    
    if( aInd >= 0 ) {
        aPos = inPlayer->pathToDest[ aInd ];
        }
    else {
        aPos.x = inPlayer->xs;
        aPos.y = inPlayer->ys;
        }
    
    double bMix = c - aInd;
    
    doublePair result = { (double)aPos.x, (double)aPos.y };
    
    if( bMix > 0 ) {
        GridPos bPos = inPlayer->pathToDest[ bInd ];
        
        double aMix = 1.0 - bMix;
        
        result.x *= aMix;
        result.y *= aMix;
        
        result.x += bMix * bPos.x;
        result.y += bMix * bPos.y;
        }
    
    return result;
    }




// if inOverrideC > -2, then it is used instead of current partial move step
GridPos computePartialMoveSpot( LiveObject *inPlayer, int inOverrideC = -2 ) {

    int c = inOverrideC;
    if( c < -1 ) {
        c = computePartialMovePathStep( inPlayer );
        }
    
    if( c >= 0 ) {
        
        GridPos cPos = inPlayer->pathToDest[c];
        
        return cPos;
        }
    else {
        GridPos cPos = { inPlayer->xs, inPlayer->ys };
        
        return cPos;
        }
    }



GridPos getPlayerPos( LiveObject *inPlayer ) {
    if( inPlayer->xs == inPlayer->xd &&
        inPlayer->ys == inPlayer->yd ) {
        
        GridPos cPos = { inPlayer->xs, inPlayer->ys };
        
        return cPos;
        }
    else {
        return computePartialMoveSpot( inPlayer );
        }
    }





static void restockPostWindowFamilies() {
    // take stock of families
    familyNamesAfterEveWindow.deallocateStringElements();
    familyLineageEveIDsAfterEveWindow.deleteAll();
    familyCountsAfterEveWindow.deleteAll();
    nextBabyFamilyIndex = 0;
    
    int barrierRadius = SettingsManager::getIntSetting( "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( "barrierOn", 1 );


    if( postWindowFamilyLogFile != NULL ) {
        fclose( postWindowFamilyLogFile );
        }
    char *fileName = autoSprintf( "%.f_familyPopLog.txt", Time::timeSec() );
    
    File folder( NULL, "familyPopLogs" );
    
    if( ! folder.exists() ) {
        folder.makeDirectory();
        }

    File *logFile = folder.getChildFile( fileName );
    delete [] fileName;
    
    char *fullPath = logFile->getFullFileName();
    delete logFile;
    
    postWindowFamilyLogFile = fopen( fullPath, "w" );
    
    delete [] fullPath;
    

    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( ! o->error &&
            ! o->isTutorial &&
            o->curseStatus.curseLevel == 0 &&
            familyLineageEveIDsAfterEveWindow.getElementIndex( 
                o->lineageEveID ) == -1 ) {
            // haven't seen this family before

            if( barrierOn ) {
                // only fams inside the barrier
                GridPos pos = getPlayerPos( o );
                
                if( abs( pos.x ) >= barrierRadius ||
                    abs( pos.y ) >= barrierRadius ) {
                    // player outside barrier
                    continue;
                    }
                }

            familyLineageEveIDsAfterEveWindow.push_back( 
                o->lineageEveID );

            char *nameCopy = NULL;

            if( o->familyName != NULL ) {
                nameCopy = stringDuplicate( o->familyName );
                }
            else {
                // don't skip tracking families that have no names
                nameCopy = autoSprintf( "UNNAMED_%d", o->lineageEveID );
                }
            
            familyNamesAfterEveWindow.push_back( nameCopy );
        

            // start with estimate of one person per family
            familyCountsAfterEveWindow.push_back( 1 );
            
            
            if( postWindowFamilyLogFile != NULL ) {
                fprintf( postWindowFamilyLogFile, "\"%s\" ", nameCopy );
                }
            }
        }
    
    if( postWindowFamilyLogFile != NULL ) {
        fprintf( postWindowFamilyLogFile, "\n" );
        }
    }



static void logFamilyCounts() {
    if( postWindowFamilyLogFile != NULL ) {
        int barrierRadius = 
            SettingsManager::getIntSetting( "barrierRadius", 250 );
        int barrierOn = SettingsManager::getIntSetting( "barrierOn", 1 );
        

        fprintf( postWindowFamilyLogFile, "%.2f ", Time::getCurrentTime() );
        
        for( int i=0; i<familyLineageEveIDsAfterEveWindow.size(); i++ ) {
            int lineageEveID = 
                familyLineageEveIDsAfterEveWindow.getElementDirect( i );
            
            int count = 0;

            for( int p=0; p<players.size(); p++ ) {
                LiveObject *o = players.getElement( p );
                
                if( ! o->error &&
                    ! o->isTutorial &&
                    o->curseStatus.curseLevel == 0 &&
                    o->lineageEveID == lineageEveID ) {
                    
                    if( barrierOn ) {
                        GridPos pos = getPlayerPos( o );
                        
                        if( abs( pos.x ) >= barrierRadius ||
                            abs( pos.y ) >= barrierRadius ) {
                            // player outside barrier
                            continue;
                            }
                        }
                    count++;
                    }
                }
            fprintf( postWindowFamilyLogFile, "%d ", count );
            // remember it
            *( familyCountsAfterEveWindow.getElement( i ) ) = count;
            }
        
        fprintf( postWindowFamilyLogFile, "\n" );
        }
    }



static int getNextBabyFamilyLineageEveIDRoundRobin() {
    nextBabyFamilyIndex++;

    if( nextBabyFamilyIndex >= familyCountsAfterEveWindow.size() ) {
        nextBabyFamilyIndex = 0;
        }

    // skip dead fams and wrap around
    char wrapOnce = false;
    while( familyCountsAfterEveWindow.
           getElementDirect( nextBabyFamilyIndex ) == 0 ) {
        nextBabyFamilyIndex++;
        
        if( nextBabyFamilyIndex >= familyCountsAfterEveWindow.size() ) {
            if( wrapOnce ) {
                // already wrapped?
                return -1;
                }
            nextBabyFamilyIndex = 0;
            wrapOnce = true;
            }
        }

    return familyLineageEveIDsAfterEveWindow.
        getElementDirect( nextBabyFamilyIndex );
    }




GridPos killPlayer( const char *inEmail ) {
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( strcmp( o->email, inEmail ) == 0 ) {
            o->error = true;
            
            return computePartialMoveSpot( o );
            }
        }
    
    GridPos noPos = { 0, 0 };
    return noPos;
    }



void forcePlayerAge( const char *inEmail, double inAge ) {
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( strcmp( o->email, inEmail ) == 0 ) {
            double ageSec = inAge / getAgeRate();
            
            o->lifeStartTimeSeconds = Time::getCurrentTime() - ageSec;
            o->needsUpdate = true;
            }
        }
    }



double computeAge( LiveObject *inPlayer );


double computeFoodDecrementTimeSeconds( LiveObject *inPlayer ) {
    double value = maxFoodDecrementSeconds * 2 * inPlayer->heat;
    
    if( value > maxFoodDecrementSeconds ) {
        // also reduce if too hot (above 0.5 heat)
        
        double extra = value - maxFoodDecrementSeconds;

        value = maxFoodDecrementSeconds - extra;
        }
    
    // all player temp effects push us up above min
    value += minFoodDecrementSeconds;

    inPlayer->indoorBonusTime = 0;
    
    if( inPlayer->isIndoors &&
        inPlayer->indoorBonusFraction > 0 &&
        computeAge( inPlayer ) > defaultActionAge ) {
        
        // non-babies get a bonus for being indoors
        inPlayer->indoorBonusTime = 
            indoorFoodDecrementSecondsBonus *
            inPlayer->indoorBonusFraction;
        
        value += inPlayer->indoorBonusTime;
        }
    
    inPlayer->foodDrainTime = value;

    return value;
    }


double getAgeRate() {
    return 1.0 / secondsPerYear;
    }


static void setDeathReason( LiveObject *inPlayer, const char *inTag,
                            int inOptionalID = 0 ) {
    
    if( inPlayer->deathReason != NULL ) {
        delete [] inPlayer->deathReason;
        }
    
    // leave space in front so it works at end of PU line
    if( strcmp( inTag, "killed" ) == 0 ||
        strcmp( inTag, "succumbed" ) == 0 ) {
        
        inPlayer->deathReason = autoSprintf( " reason_%s_%d", 
                                             inTag, inOptionalID );
        }
    else {
        // ignore ID
        inPlayer->deathReason = autoSprintf( " reason_%s", inTag );
        }
    }



int longestShutdownLine = -1;

void handleShutdownDeath( LiveObject *inPlayer,
                          int inX, int inY ) {
    if( inPlayer->curseStatus.curseLevel == 0 &&
        inPlayer->parentChainLength > longestShutdownLine ) {
        
        // never count a cursed player as a long line
        
        longestShutdownLine = inPlayer->parentChainLength;
        
        FILE *f = fopen( "shutdownLongLineagePos.txt", "w" );
        if( f != NULL ) {
            fprintf( f, "%d,%d", inX, inY );
            fclose( f );
            }
        }
    }



double computeAge( double inLifeStartTimeSeconds ) {
    
    double deltaSeconds = 
        Time::getCurrentTime() - inLifeStartTimeSeconds;
    
    double age = deltaSeconds * getAgeRate();
    
    return age;
    }



double computeAge( LiveObject *inPlayer ) {
    double age = computeAge( inPlayer->lifeStartTimeSeconds );
    if( age >= forceDeathAge ) {
        setDeathReason( inPlayer, "age" );
        
        inPlayer->error = true;
        
        age = forceDeathAge;
        }
    return age;
    }



int getSayLimit( LiveObject *inPlayer ) {
    int limit = (unsigned int)( floor( computeAge( inPlayer ) ) + 1 );

    if( inPlayer->isEve && limit < 30 ) {
        // give Eve room to name her family line
        limit = 30;
        }
    return limit;
    }




int getSecondsPlayed( LiveObject *inPlayer ) {
    double deltaSeconds = 
        Time::getCurrentTime() - inPlayer->trueStartTimeSeconds;

    return lrint( deltaSeconds );
    }


// false for male, true for female
char getFemale( LiveObject *inPlayer ) {
    ObjectRecord *r = getObject( inPlayer->displayID );
    
    return ! r->male;
    }


static int getFirstFertileAge() {
    return 14;
    }


char isFertileAge( LiveObject *inPlayer ) {
    double age = computeAge( inPlayer );
                    
    char f = getFemale( inPlayer );
                    
    if( age >= getFirstFertileAge() && age <= 40 && f ) {
        return true;
        }
    else {
        return false;
        }
    }




int computeFoodCapacity( LiveObject *inPlayer ) {
    int ageInYears = lrint( computeAge( inPlayer ) );
    
    int returnVal = 0;
    
    if( ageInYears < 44 ) {
        
        if( ageInYears > 16 ) {
            ageInYears = 16;
            }
        
        returnVal = ageInYears + 4;
        }
    else {
        // food capacity decreases as we near 60
        int cap = 60 - ageInYears + 4;
        
        if( cap < 4 ) {
            cap = 4;
            }
        
        int lostBars = 20 - cap;

        if( lostBars > 0 && inPlayer->fitnessScore > 0 ) {
        
            // consider effect of fitness on reducing lost bars

            // for now, let's make it quadratic
            double maxLostBars = 
                16 - 16 * pow( inPlayer->fitnessScore / 60.0, 2 );
            
            if( lostBars > maxLostBars ) {
                lostBars = maxLostBars;
                }

            if( lostBars < 0 ) {
                lostBars = 0;
                }
            }
        
        returnVal = 20 - lostBars;
        }

    return ceil( returnVal * inPlayer->foodCapModifier );
    }



int computeOverflowFoodCapacity( int inBaseCapacity ) {
    // even littlest baby has +1 overflow, to get everyone used to the
    // concept.
    // by adulthood (when base cap is 20), overflow cap is 90.6
    return 1 + pow( inBaseCapacity, 8 ) * 0.0000000035;
    }



static void drinkAlcohol( LiveObject *inPlayer, int inAlcoholAmount ) {
    double doneGrowingAge = 16;
    
    double multiplier = 1.0;
    

    double age = computeAge( inPlayer );
    
    // alcohol affects a baby 2x
    // affects an 8-y-o 1.5x
    if( age < doneGrowingAge ) {
        multiplier += 1.0 - age / doneGrowingAge;
        }

    double amount = inAlcoholAmount * multiplier;
    
    inPlayer->drunkenness += amount;
    }



char *slurSpeech( int inSpeakerID,
                  char *inTranslatedPhrase, double inDrunkenness ) {
    char *working = stringDuplicate( inTranslatedPhrase );
    
    char *starPos = strstr( working, " *" );

    char *extraData = NULL;
    
    if( starPos != NULL ) {
        extraData = stringDuplicate( starPos );
        starPos[0] = '\0';
        }
    
    SimpleVector<char> slurredChars;
    
    // 1 in 10 letters slurred with 1 drunkenness
    // all characters slurred with 10 drunkenness
    double baseSlurChance = 0.1;
    
    double slurChance = baseSlurChance * inDrunkenness;

    // 2 in 10 words mixed up in order with 6 drunkenness
    // all words mixed up at 10 drunkenness
    double baseWordSwapChance = 0.1;

    // but don't start mixing up words at all until 6 drunkenness
    // thus, the 0 to 100% mix up range is from 6 to 10 drunkenness
    double wordSwapChance = 2 * baseWordSwapChance * ( inDrunkenness - 5 );



    // first, swap word order
    SimpleVector<char *> *words = tokenizeString( working );

    // always slurr exactly the same for a given speaker
    // repeating the same phrase won't keep remapping
    // but map different length phrases differently
    JenkinsRandomSource slurRand( inSpeakerID + 
                                  words->size() + 
                                  inDrunkenness );
    

    for( int i=0; i<words->size(); i++ ) {
        if( slurRand.getRandomBoundedDouble( 0, 1 ) < wordSwapChance ) {
            char *temp = words->getElementDirect( i );
            
            // possible swap distance based on drunkenness
            
            // again, don't start reording words until 6 drunkenness
            int maxDist = inDrunkenness - 5;

            if( maxDist >= words->size() - i ) {
                maxDist = words->size() - i - 1;
                }
            
            if( maxDist > 0 ) {
                int jump = slurRand.getRandomBoundedInt( 0, maxDist );
            
                
                *( words->getElement( i ) ) = 
                    words->getElementDirect( i + jump );
            
                *( words->getElement( i + jump ) ) = temp;
                }
            }
        }
    

    char **allWords = words->getElementArray();
    char *wordsTogether = join( allWords, words->size(), " " );
    
    words->deallocateStringElements();
    delete words;
    
    delete [] allWords;

    delete [] working;
    
    working = wordsTogether;


    int len = strlen( working );
    for( int i=0; i<len; i++ ) {
        char c = working[i];
        
        slurredChars.push_back( c );

        if( c < 'A' || c > 'Z' ) {
            // only A-Z, no slurred punctuation
            continue;
            }

        if( slurRand.getRandomBoundedDouble( 0, 1 ) < slurChance ) {
            slurredChars.push_back( c );
            }
        }

    delete [] working;
    
    if( extraData != NULL ) {
        slurredChars.appendElementString( extraData );
        delete [] extraData;
        }
    

    return slurredChars.getElementString();
    }





// with 128-wide tiles, character moves at 480 pixels per second
// at 60 fps, this is 8 pixels per frame
// important that it's a whole number of pixels for smooth camera movement
static double baseWalkSpeed = 3.75;

// min speed for takeoff
static double minFlightSpeed = 15;



double computeMoveSpeed( LiveObject *inPlayer ) {
    double age = computeAge( inPlayer );
    

    double speed = baseWalkSpeed;
    
    // baby moves at 360 pixels per second, or 6 pixels per frame
    double babySpeedFactor = 0.75;

    double fullSpeedAge = 10.0;
    

    if( age < fullSpeedAge ) {
        
        double speedFactor = babySpeedFactor + 
            ( 1.0 - babySpeedFactor ) * age / fullSpeedAge;
        
        speed *= speedFactor;
        }


    // for now, try no age-based speed decrease
    /*
    if( age < 20 ) {
        speed *= age / 20;
        }
    if( age > 40 ) {
        // half speed by 60, then keep slowing down after that
        speed -= (age - 40 ) * 2.0 / 20.0;
        
        }
    */
    // no longer slow down with hunger
    /*
    int foodCap = computeFoodCapacity( inPlayer );
    
    
    if( inPlayer->foodStore <= foodCap / 2 ) {
        // jumps instantly to 1/2 speed at half food, then decays after that
        speed *= inPlayer->foodStore / (double) foodCap;
        }
    */



    // apply character's speed mult
    speed *= getObject( inPlayer->displayID )->speedMult;
    

    char riding = false;
    
    if( inPlayer->holdingID > 0 ) {
        ObjectRecord *r = getObject( inPlayer->holdingID );

        if( r->clothing == 'n' ) {
            // clothing only changes your speed when it's worn
            speed *= r->speedMult;
            }
        
        if( r->rideable ) {
            riding = true;
            }
        }
    

    if( !riding ) {
        // clothing can affect speed

        for( int i=0; i<NUM_CLOTHING_PIECES; i++ ) {
            ObjectRecord *c = clothingByIndex( inPlayer->clothing, i );
            
            if( c != NULL ) {
                
                speed *= c->speedMult;
                }
            }
        }

    if( inPlayer->killPosseSize > 0 ) {
        // player part of a posse
        double posseSpeedMult = 1.0;
        
        if( inPlayer->killPosseSize <= 4 ) {
            posseSpeedMult = 
                posseSizeSpeedMultipliers[ inPlayer->killPosseSize - 1 ];
            }
        else {
            // 4+ same value as 4
            posseSpeedMult = posseSizeSpeedMultipliers[3];
            }
        
        if( inPlayer->isTwin ) {
            // twins always run at slowest speed when trying to kill
            // they can't form their own posse, but can join
            // into posses to help speed up others
            posseSpeedMult = posseSizeSpeedMultipliers[0];
            }

        speed *= posseSpeedMult;
        }
    

    // never move at 0 speed, divide by 0 errors for eta times
    if( speed < 0.01 ) {
        speed = 0.01;
        }

    
    // after all multipliers, make sure it's a whole number of pixels per frame

    double pixelsPerFrame = speed * 128.0 / 60.0;
    
    
    if( pixelsPerFrame > 0.5 ) {
        // can round to at least one pixel per frame
        pixelsPerFrame = lrint( pixelsPerFrame );
        }
    else {
        // fractional pixels per frame
        
        // ensure a whole number of frames per pixel
        double framesPerPixel = 1.0 / pixelsPerFrame;
        
        framesPerPixel = lrint( framesPerPixel );
        
        pixelsPerFrame = 1.0 / framesPerPixel;
        }
    
    speed = pixelsPerFrame * 60 / 128.0;
        
    return speed;
    }







static float sign( float inF ) {
    if (inF > 0) return 1;
    if (inF < 0) return -1;
    return 0;
    }


// how often do we check what a player is standing on top of for attack effects?
static double playerCrossingCheckStepTime = 0.25;


// for steps in main loop that shouldn't happen every loop
// (loop runs faster or slower depending on how many messages are incoming)
static double periodicStepTime = 0.25;
static double lastPeriodicStepTime = 0;




// recompute heat for fixed number of players per timestep
static int numPlayersRecomputeHeatPerStep = 8;
static int lastPlayerIndexHeatRecomputed = -1;
static double lastHeatUpdateTime = 0;
static double heatUpdateTimeStep = 0.1;


// how often the player's personal heat advances toward their environmental
// heat value
static double heatUpdateSeconds = 2;


// air itself offers some insulation
// a vacuum panel has R-value that is 25x greater than air
static float rAir = 0.04;



// blend R-values multiplicatively, for layers
// 1 - R( A + B ) = (1 - R(A)) * (1 - R(B))
//
// or
//
//R( A + B ) =  R(A) + R(B) - R(A) * R(B)
static double rCombine( double inRA, double inRB ) {
    return inRA + inRB - inRA * inRB;
    }




static float computeClothingR( LiveObject *inPlayer ) {
    
    float headWeight = 0.25;
    float chestWeight = 0.35;
    float buttWeight = 0.2;
    float eachFootWeigth = 0.1;
            
    float backWeight = 0.1;


    float clothingR = 0;
            
    if( inPlayer->clothing.hat != NULL ) {
        clothingR += headWeight *  inPlayer->clothing.hat->rValue;
        }
    if( inPlayer->clothing.tunic != NULL ) {
        clothingR += chestWeight * inPlayer->clothing.tunic->rValue;
        }
    if( inPlayer->clothing.frontShoe != NULL ) {
        clothingR += 
            eachFootWeigth * inPlayer->clothing.frontShoe->rValue;
        }
    if( inPlayer->clothing.backShoe != NULL ) {
        clothingR += eachFootWeigth * 
            inPlayer->clothing.backShoe->rValue;
        }
    if( inPlayer->clothing.bottom != NULL ) {
        clothingR += buttWeight * inPlayer->clothing.bottom->rValue;
        }
    if( inPlayer->clothing.backpack != NULL ) {
        clothingR += backWeight * inPlayer->clothing.backpack->rValue;
        }
    
    // even if the player is naked, they are insulated from their
    // environment by rAir
    return rCombine( rAir, clothingR );
    }



static float computeClothingHeat( LiveObject *inPlayer ) {
    // clothing can contribute heat
    // apply this separate from heat grid above
    float clothingHeat = 0;
    for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
                
        ObjectRecord *cO = clothingByIndex( inPlayer->clothing, c );
            
        if( cO != NULL ) {
            clothingHeat += cO->heatValue;

            // contained items in clothing can contribute
            // heat, shielded by clothing r-values
            double cRFactor = 1 - cO->rValue;

            for( int s=0; 
                 s < inPlayer->clothingContained[c].size(); s++ ) {
                        
                ObjectRecord *sO = 
                    getObject( inPlayer->clothingContained[c].
                               getElementDirect( s ) );
                        
                clothingHeat += 
                    sO->heatValue * cRFactor;
                }
            }
        }
    return clothingHeat;
    }



static float computeHeldHeat( LiveObject *inPlayer ) {
    float heat = 0;
    
    // what player is holding can contribute heat
    // add this to the grid, since it's "outside" the player's body
    if( inPlayer->holdingID > 0 ) {
        ObjectRecord *heldO = getObject( inPlayer->holdingID );
                
        heat += heldO->heatValue;
                
        double heldRFactor = 1 - heldO->rValue;
                
        // contained can contribute too, but shielded by r-value
        // of container
        for( int c=0; c<inPlayer->numContained; c++ ) {
                    
            int cID = inPlayer->containedIDs[c];
            char hasSub = false;
                    
            if( cID < 0 ) {
                hasSub = true;
                cID = -cID;
                }

            ObjectRecord *contO = getObject( cID );
                    
            heat += 
                contO->heatValue * heldRFactor;
                    

            if( hasSub ) {
                // sub contained too, but shielded by both r-values
                double contRFactor = 1 - contO->rValue;

                for( int s=0; 
                     s<inPlayer->subContainedIDs[c].size(); s++ ) {
                        
                    ObjectRecord *subO =
                        getObject( inPlayer->subContainedIDs[c].
                                   getElementDirect( s ) );
                            
                    heat += 
                        subO->heatValue * 
                        contRFactor * heldRFactor;
                    }
                }
            }
        }
    return heat;
    }




static void recomputeHeatMap( LiveObject *inPlayer ) {
    
    int gridSize = HEAT_MAP_D * HEAT_MAP_D;

    // assume indoors until we find an air boundary of space
    inPlayer->isIndoors = true;
    

    // what if we recompute it from scratch every time?
    for( int i=0; i<gridSize; i++ ) {
        inPlayer->heatMap[i] = 0;
        }

    float heatOutputGrid[ HEAT_MAP_D * HEAT_MAP_D ];
    float rGrid[ HEAT_MAP_D * HEAT_MAP_D ];
    float rFloorGrid[ HEAT_MAP_D * HEAT_MAP_D ];


    GridPos pos = getPlayerPos( inPlayer );


    // held baby's pos matches parent pos
    if( inPlayer->heldByOther ) {
        LiveObject *parentObject = getLiveObject( inPlayer->heldByOtherID );
        
        if( parentObject != NULL ) {
            pos = getPlayerPos( parentObject );
            }
        } 

    
    

    for( int y=0; y<HEAT_MAP_D; y++ ) {
        int mapY = pos.y + y - HEAT_MAP_D / 2;
                
        for( int x=0; x<HEAT_MAP_D; x++ ) {
                    
            int mapX = pos.x + x - HEAT_MAP_D / 2;
                    
            int j = y * HEAT_MAP_D + x;
            heatOutputGrid[j] = 0;
            rGrid[j] = rAir;
            rFloorGrid[j] = rAir;


            // call Raw version for better performance
            // we don't care if object decayed since we last looked at it
            ObjectRecord *o = getObject( getMapObjectRaw( mapX, mapY ) );
                    
                    
                    

            if( o != NULL ) {
                heatOutputGrid[j] += o->heatValue;
                if( o->permanent ) {
                    // loose objects sitting on ground don't
                    // contribute to r-value (like dropped clothing)
                    rGrid[j] = rCombine( rGrid[j], o->rValue );
                    }


                // skip checking for heat-producing contained items
                // for now.  Consumes too many server-side resources
                // can still check for heat produced by stuff in
                // held container (below).
                        
                if( false && o->numSlots > 0 ) {
                    // contained can produce heat shielded by container
                    // r value
                    double oRFactor = 1 - o->rValue;
                            
                    int numCont;
                    int *cont = getContained( mapX, mapY, &numCont );
                            
                    if( cont != NULL ) {
                                
                        for( int c=0; c<numCont; c++ ) {
                                    
                            int cID = cont[c];
                            char hasSub = false;
                            if( cID < 0 ) {
                                hasSub = true;
                                cID = -cID;
                                }

                            ObjectRecord *cO = getObject( cID );
                            heatOutputGrid[j] += 
                                cO->heatValue * oRFactor;
                                    
                            if( hasSub ) {
                                double cRFactor = 1 - cO->rValue;
                                        
                                int numSub;
                                int *sub = getContained( mapX, mapY, 
                                                         &numSub, 
                                                         c + 1 );
                                if( sub != NULL ) {
                                    for( int s=0; s<numSub; s++ ) {
                                        ObjectRecord *sO = 
                                            getObject( sub[s] );
                                                
                                        heatOutputGrid[j] += 
                                            sO->heatValue * 
                                            cRFactor * 
                                            oRFactor;
                                        }
                                    delete [] sub;
                                    }
                                }
                            }
                        delete [] cont;
                        }
                    }
                }
                    

            // floor can insulate or produce heat too
            ObjectRecord *fO = getObject( getMapFloor( mapX, mapY ) );
                    
            if( fO != NULL ) {
                heatOutputGrid[j] += fO->heatValue;
                rFloorGrid[j] = rCombine( rFloorGrid[j], fO->rValue );
                }
            }
        }


    
    int numNeighbors = 8;
    int ndx[8] = { 0, 1,  0, -1,  1,  1, -1, -1 };
    int ndy[8] = { 1, 0, -1,  0,  1, -1,  1, -1 };
    
            
    int playerMapIndex = 
        ( HEAT_MAP_D / 2 ) * HEAT_MAP_D +
        ( HEAT_MAP_D / 2 );
        

    
            
    heatOutputGrid[ playerMapIndex ] += computeHeldHeat( inPlayer );
    

    // grid of flags for points that are in same airspace (surrounded by walls)
    // as player
    // This is the area where heat spreads evenly by convection
    char airSpaceGrid[ HEAT_MAP_D * HEAT_MAP_D ];
    
    memset( airSpaceGrid, false, HEAT_MAP_D * HEAT_MAP_D );
    
    airSpaceGrid[ playerMapIndex ] = true;

    SimpleVector<int> frontierA;
    SimpleVector<int> frontierB;
    frontierA.push_back( playerMapIndex );
    
    SimpleVector<int> *thisFrontier = &frontierA;
    SimpleVector<int> *nextFrontier = &frontierB;

    while( thisFrontier->size() > 0 ) {

        for( int f=0; f<thisFrontier->size(); f++ ) {
            
            int i = thisFrontier->getElementDirect( f );
            
            char negativeYCutoff = false;
            
            if( rGrid[i] > rAir ) {
                // grid cell is insulating, and somehow it's in our
                // frontier.  Player must be standing behind a closed
                // door.  Block neighbors to south
                negativeYCutoff = true;
                }
            

            int x = i % HEAT_MAP_D;
            int y = i / HEAT_MAP_D;
            
            for( int n=0; n<numNeighbors; n++ ) {
                        
                int nx = x + ndx[n];
                int ny = y + ndy[n];
                
                if( negativeYCutoff && ndy[n] < 1 ) {
                    continue;
                    }

                if( nx >= 0 && nx < HEAT_MAP_D &&
                    ny >= 0 && ny < HEAT_MAP_D ) {

                    int nj = ny * HEAT_MAP_D + nx;
                    
                    if( ! airSpaceGrid[ nj ]
                        && rGrid[ nj ] <= rAir ) {

                        airSpaceGrid[ nj ] = true;
                        
                        nextFrontier->push_back( nj );
                        }
                    }
                }
            }
        
        thisFrontier->deleteAll();
        
        SimpleVector<int> *temp = thisFrontier;
        thisFrontier = nextFrontier;
        
        nextFrontier = temp;
        }
    
    if( rGrid[playerMapIndex] > rAir ) {
        // player standing in insulated spot
        // don't count this as part of their air space
        airSpaceGrid[ playerMapIndex ] = false;
        }

    int numInAirspace = 0;
    for( int i=0; i<gridSize; i++ ) {
        if( airSpaceGrid[ i ] ) {
            numInAirspace++;
            }
        }
    
    
    float rBoundarySum = 0;
    int rBoundarySize = 0;
    
    for( int i=0; i<gridSize; i++ ) {
        if( airSpaceGrid[i] ) {
            
            int x = i % HEAT_MAP_D;
            int y = i / HEAT_MAP_D;
            
            for( int n=0; n<numNeighbors; n++ ) {
                        
                int nx = x + ndx[n];
                int ny = y + ndy[n];
                
                if( nx >= 0 && nx < HEAT_MAP_D &&
                    ny >= 0 && ny < HEAT_MAP_D ) {
                    
                    int nj = ny * HEAT_MAP_D + nx;
                    
                    if( ! airSpaceGrid[ nj ] ) {
                        
                        // boundary!
                        rBoundarySum += rGrid[ nj ];
                        rBoundarySize ++;
                        }
                    }
                else {
                    // boundary is off edge
                    // assume air R-value
                    rBoundarySum += rAir;
                    rBoundarySize ++;
                    inPlayer->isIndoors = false;
                    }
                }
            }
        }

    
    // floor counts as boundary too
    // 4x its effect (seems more important than one of 8 walls
    
    // count non-air floor tiles while we're at it
    int numFloorTilesInAirspace = 0;

    if( numInAirspace > 0 ) {
        for( int i=0; i<gridSize; i++ ) {
            if( airSpaceGrid[i] ) {
                rBoundarySum += 4 * rFloorGrid[i];
                rBoundarySize += 4;
                
                if( rFloorGrid[i] > rAir ) {
                    numFloorTilesInAirspace++;
                    }
                else {
                    // gap in floor
                    inPlayer->isIndoors = false;
                    }
                }
            }
        }
    


    float rBoundaryAverage = rAir;
    
    if( rBoundarySize > 0 ) {
        rBoundaryAverage = rBoundarySum / rBoundarySize;
        }

    if( inPlayer->isIndoors ) {
        // the more insulating the boundary, the bigger the bonus
        inPlayer->indoorBonusFraction = rBoundaryAverage;
        }
    
    



    float airSpaceHeatSum = 0;
    
    for( int i=0; i<gridSize; i++ ) {
        if( airSpaceGrid[i] ) {
            airSpaceHeatSum += heatOutputGrid[i];
            }
        }


    float airSpaceHeatVal = 0;
    
    if( numInAirspace > 0 ) {
        // spread heat evenly over airspace
        airSpaceHeatVal = airSpaceHeatSum / numInAirspace;
        }

    float containedAirSpaceHeatVal = airSpaceHeatVal * rBoundaryAverage;
    


    float radiantAirSpaceHeatVal = 0;

    GridPos playerHeatMapPos = { playerMapIndex % HEAT_MAP_D, 
                                 playerMapIndex / HEAT_MAP_D };
    

    int numRadiantHeatSources = 0;
    
    for( int i=0; i<gridSize; i++ ) {
        if( airSpaceGrid[i] && heatOutputGrid[i] > 0 ) {
            
            int x = i % HEAT_MAP_D;
            int y = i / HEAT_MAP_D;
            
            GridPos heatPos = { x, y };
            

            double d = distance( playerHeatMapPos, heatPos );
            
            // avoid infinite heat when player standing on source

            radiantAirSpaceHeatVal += heatOutputGrid[ i ] / ( 1.5 * d + 1 );
            numRadiantHeatSources ++;
            }
        }
    

    float biomeHeatWeight = 1;
    float radiantHeatWeight = 1;
    
    float containedHeatWeight = 4;


    // boundary r-value also limits affect of biome heat on player's
    // environment... keeps biome "out"
    float boundaryLeak = 1 - rBoundaryAverage;

    if( numFloorTilesInAirspace != numInAirspace ) {
        // biome heat invades airspace if entire thing isn't covered by
        // a floor (not really indoors)
        boundaryLeak = 1;
        }


    // a hot biome only pulls us up above perfect
    // (hot biome leaking into a building can never make the building
    //  just right).
    // Enclosed walls can make a hot biome not as hot, but never cool
    float biomeHeat = getBiomeHeatValue( getMapBiome( pos.x, pos.y ) );
    
    if( biomeHeat > targetHeat ) {
        biomeHeat = boundaryLeak * (biomeHeat - targetHeat) + targetHeat;
        }
    else if( biomeHeat < 0 ) {
        // a cold biome's coldness is modulated directly by walls, however
        biomeHeat = boundaryLeak * biomeHeat;
        }
    
    // small offset to ensure that naked-on-green biome the same
    // in new heat model as old
    float constHeatValue = 1.1;

    inPlayer->envHeat = 
        radiantHeatWeight * radiantAirSpaceHeatVal + 
        containedHeatWeight * containedAirSpaceHeatVal +
        biomeHeatWeight * biomeHeat +
        constHeatValue;

    inPlayer->biomeHeat = biomeHeat + constHeatValue;
    }




typedef struct MoveRecord {
        int playerID;
        char *formatString;
        int absoluteX, absoluteY;
    } MoveRecord;



// formatString in returned record destroyed by caller
MoveRecord getMoveRecord( LiveObject *inPlayer,
                          char inNewMovesOnly,
                          SimpleVector<ChangePosition> *inChangeVector = 
                          NULL ) {

    MoveRecord r;
    r.playerID = inPlayer->id;
    
    // p_id xs ys xd yd fraction_done eta_sec
    
    double deltaSec = Time::getCurrentTime() - inPlayer->moveStartTime;
    
    double etaSec = inPlayer->moveTotalSeconds - deltaSec;
    
    if( etaSec < 0 ) {
        etaSec = 0;
        }

    
    r.absoluteX = inPlayer->xs;
    r.absoluteY = inPlayer->ys;
            
            
    SimpleVector<char> messageLineBuffer;
    
    // start is absolute
    char *startString = autoSprintf( "%d %%d %%d %.3f %.3f %d", 
                                     inPlayer->id, 
                                     inPlayer->moveTotalSeconds, etaSec,
                                     inPlayer->pathTruncated );
    // mark that this has been sent
    inPlayer->pathTruncated = false;

    if( inNewMovesOnly ) {
        inPlayer->newMove = false;
        }

            
    messageLineBuffer.appendElementString( startString );
    delete [] startString;
            
    for( int p=0; p<inPlayer->pathLength; p++ ) {
                // rest are relative to start
        char *stepString = autoSprintf( " %d %d", 
                                        inPlayer->pathToDest[p].x
                                        - inPlayer->xs,
                                        inPlayer->pathToDest[p].y
                                        - inPlayer->ys );
        
        messageLineBuffer.appendElementString( stepString );
        delete [] stepString;
        }
    
    messageLineBuffer.appendElementString( "\n" );
    
    r.formatString = messageLineBuffer.getElementString();    
    
    if( inChangeVector != NULL ) {
        ChangePosition p = { inPlayer->xd, inPlayer->yd, false };
        inChangeVector->push_back( p );
        }

    return r;
    }



SimpleVector<MoveRecord> getMoveRecords( 
    char inNewMovesOnly,
    SimpleVector<ChangePosition> *inChangeVector = NULL ) {
    
    SimpleVector<MoveRecord> v;
    
    int numPlayers = players.size();
                
    for( int i=0; i<numPlayers; i++ ) {
                
        LiveObject *o = players.getElement( i );                
        
        if( o->error ) {
            continue;
            }

        if( ( o->xd != o->xs || o->yd != o->ys )
            &&
            ( o->newMove || !inNewMovesOnly ) ) {
            
 
            MoveRecord r = getMoveRecord( o, inNewMovesOnly, inChangeVector );
            
            v.push_back( r );
            }
        }

    return v;
    }



char *getMovesMessageFromList( SimpleVector<MoveRecord> *inMoves,
                               GridPos inRelativeToPos ) {

    int numLines = 0;
    
    SimpleVector<char> messageBuffer;

    messageBuffer.appendElementString( "PM\n" );

    for( int i=0; i<inMoves->size(); i++ ) {
        MoveRecord r = inMoves->getElementDirect(i);
        
        char *line = autoSprintf( r.formatString, 
                                  r.absoluteX - inRelativeToPos.x,
                                  r.absoluteY - inRelativeToPos.y );
        
        messageBuffer.appendElementString( line );
        delete [] line;
        
        numLines ++;
        }
    
    if( numLines > 0 ) {
        
        messageBuffer.push_back( '#' );
                
        char *message = messageBuffer.getElementString();
        
        return message;
        }
    
    return NULL;
    }



double intDist( int inXA, int inYA, int inXB, int inYB ) {
    double dx = (double)inXA - (double)inXB;
    double dy = (double)inYA - (double)inYB;

    return sqrt(  dx * dx + dy * dy );
    }
    
    
    
// returns NULL if there are no matching moves
// positions in moves relative to inRelativeToPos
// filters out moves that are taking place further than 32 away from inLocalPos
char *getMovesMessage( char inNewMovesOnly,
                       GridPos inRelativeToPos,
                       GridPos inLocalPos,
                       SimpleVector<ChangePosition> *inChangeVector = NULL ) {
    
    
    SimpleVector<MoveRecord> v = getMoveRecords( inNewMovesOnly, 
                                                 inChangeVector );
    
    SimpleVector<MoveRecord> closeRecords;

    for( int i=0; i<v.size(); i++ ) {
        MoveRecord r = v.getElementDirect( i );
        
        double d = intDist( r.absoluteX, r.absoluteY,
                            inLocalPos.x, inLocalPos.y );
        
        if( d <= 32 ) {
            closeRecords.push_back( r );
            }
        }
    
    

    char *message = getMovesMessageFromList( &closeRecords, inRelativeToPos );
    
    for( int i=0; i<v.size(); i++ ) {
        delete [] v.getElement(i)->formatString;
        }
    
    return message;
    }



static char isGridAdjacent( int inXA, int inYA, int inXB, int inYB ) {
    if( ( abs( inXA - inXB ) == 1 && inYA == inYB ) 
        ||
        ( abs( inYA - inYB ) == 1 && inXA == inXB ) ) {
        
        return true;
        }

    return false;
    }


//static char isGridAdjacent( GridPos inA, GridPos inB ) {
//    return isGridAdjacent( inA.x, inA.y, inB.x, inB.y );
//    }


static char isGridAdjacentDiag( int inXA, int inYA, int inXB, int inYB ) {
    if( isGridAdjacent( inXA, inYA, inXB, inYB ) ) {
        return true;
        }
    
    if( abs( inXA - inXB ) == 1 && abs( inYA - inYB ) == 1 ) {
        return true;
        }
    
    return false;
    }


static char isGridAdjacentDiag( GridPos inA, GridPos inB ) {
    return isGridAdjacentDiag( inA.x, inA.y, inB.x, inB.y );
    }



static char equal( GridPos inA, GridPos inB ) {
    if( inA.x == inB.x && inA.y == inB.y ) {
        return true;
        }
    return false;
    }





// returns (0,0) if no player found
GridPos getClosestPlayerPos( int inX, int inY ) {
    GridPos c = { inX, inY };
    
    double closeDist = DBL_MAX;
    GridPos closeP = { 0, 0 };
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        if( o->error ) {
            continue;
            }
        
        GridPos p;

        if( o->xs == o->xd && o->ys == o->yd ) {
            p.x = o->xd;
            p.y = o->yd;
            }
        else {
            p = computePartialMoveSpot( o );
            }
        
        double d = distance( p, c );
        
        if( d < closeDist ) {
            closeDist = d;
            closeP = p;
            }
        }
    return closeP;
    }




static int chunkDimensionX = 32;
static int chunkDimensionY = 30;

static int maxSpeechRadius = 16;


static int getMaxChunkDimension() {
    return chunkDimensionX;
    }


static SocketPoll sockPoll;



static void setPlayerDisconnected( LiveObject *inPlayer, 
                                   const char *inReason ) {    
    /*
    setDeathReason( inPlayer, "disconnected" );
    
    inPlayer->error = true;
    inPlayer->errorCauseString = inReason;
    */
    // don't kill them
    
    // just mark them as not connected

    AppLog::infoF( "Player %d (%s) marked as disconnected (%s).",
                   inPlayer->id, inPlayer->email, inReason );
    inPlayer->connected = false;

    // when player reconnects, they won't get a force PU message
    // so we shouldn't be waiting for them to ack
    inPlayer->waitingForForceResponse = false;


    if( inPlayer->vogMode ) {    
        inPlayer->vogMode = false;
                        
        GridPos p = inPlayer->preVogPos;
        
        inPlayer->xd = p.x;
        inPlayer->yd = p.y;
        
        inPlayer->xs = p.x;
        inPlayer->ys = p.y;

        inPlayer->birthPos = inPlayer->preVogBirthPos;
        }
    
    
    if( inPlayer->sock != NULL ) {
        // also, stop polling their socket, which will trigger constant
        // socket events from here on out, and cause us to busy-loop
        sockPoll.removeSocket( inPlayer->sock );

        delete inPlayer->sock;
        inPlayer->sock = NULL;
        }
    if( inPlayer->sockBuffer != NULL ) {
        delete inPlayer->sockBuffer;
        inPlayer->sockBuffer = NULL;
        }
    }



// if inOnePlayerOnly set, we only send to that player
static void sendGlobalMessage( char *inMessage,
                               LiveObject *inOnePlayerOnly = NULL ) {
    char found;
    char *noSpaceMessage = replaceAll( inMessage, " ", "_", &found );

    char *fullMessage = autoSprintf( "MS\n%s\n#", noSpaceMessage );
    
    delete [] noSpaceMessage;

    int len = strlen( fullMessage );
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( inOnePlayerOnly != NULL && o != inOnePlayerOnly ) {
            continue;
            }

        if( ! o->error && ! o->isTutorial && o->connected ) {
            int numSent = 
                o->sock->send( (unsigned char*)fullMessage, 
                               len, 
                               false, false );
        
            if( numSent != len ) {
                setPlayerDisconnected( o, "Socket write failed" );
                }
            }
        }
    delete [] fullMessage;
    }



typedef struct WarPeaceMessageRecord {
        char war;
        int lineageAEveID;
        int lineageBEveID;
        double t;
    } WarPeaceMessageRecord;

SimpleVector<WarPeaceMessageRecord> warPeaceRecords;



void sendPeaceWarMessage( const char *inPeaceOrWar,
                          char inWar,
                          int inLineageAEveID, int inLineageBEveID ) {
    
    double curTime = Time::getCurrentTime();
    
    for( int i=0; i<warPeaceRecords.size(); i++ ) {
        WarPeaceMessageRecord *r = warPeaceRecords.getElement( i );
        
        if( inWar != r->war ) {
            continue;
            }
        
        if( ( r->lineageAEveID == inLineageAEveID &&
              r->lineageBEveID == inLineageBEveID )
            ||
            ( r->lineageAEveID == inLineageBEveID &&
              r->lineageBEveID == inLineageAEveID ) ) {

            if( r->t > curTime - 3 * 60 ) {
                // stil fresh, last similar message happened
                // less than three minutes ago
                return;
                }
            else {
                // stale
                // remove it
                warPeaceRecords.deleteElement( i );
                break;
                }
            }
        }
    WarPeaceMessageRecord r = { inWar, inLineageAEveID, inLineageBEveID,
                                curTime };
    warPeaceRecords.push_back( r );


    const char *nameA = "NAMELESS";
    const char *nameB = "NAMELESS";
    
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *o = players.getElement( j );
                        
        if( ! o->error && 
            o->lineageEveID == inLineageAEveID &&
            o->familyName != NULL ) {
            nameA = o->familyName;
            break;
            }
        }
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *o = players.getElement( j );
                        
        if( ! o->error && 
            o->lineageEveID == inLineageBEveID &&
            o->familyName != NULL ) {
            nameB = o->familyName;
            break;
            }
        }

    char *message = autoSprintf( "%s BETWEEN %s**AND %s FAMILIES",
                                 inPeaceOrWar,
                                 nameA, nameB );

    sendGlobalMessage( message );
    
    delete [] message;
    }




void checkCustomGlobalMessage() {
    
    if( ! SettingsManager::getIntSetting( "customGlobalMessageOn", 0 ) ) {
        return;
        }


    double spacing = 
        SettingsManager::getDoubleSetting( 
            "customGlobalMessageSecondsSpacing", 10.0 );
    
    double lastTime = 
        SettingsManager::getDoubleSetting( 
            "customGlobalMessageLastSendTime", 0.0 );

    double curTime = Time::getCurrentTime();
    
    if( curTime - lastTime < spacing ) {
        return;
        }
        

    
    // check if there's a new custom message waiting
    char *message = 
        SettingsManager::getSettingContents( "customGlobalMessage", 
                                             "" );
    
    if( strcmp( message, "" ) != 0 ) {
        

        int numLines;
        
        char **lines = split( message, "\n", &numLines );
        
        int nextLine = 
            SettingsManager::getIntSetting( 
                "customGlobalMessageNextLine", 0 );
        
        if( nextLine < numLines ) {
            sendGlobalMessage( lines[nextLine] );
            
            nextLine++;
            SettingsManager::setSetting( 
                "customGlobalMessageNextLine", nextLine );

            SettingsManager::setDoubleSetting( 
                "customGlobalMessageLastSendTime", curTime );
            }
        else {
            // out of lines
            SettingsManager::setSetting( "customGlobalMessageOn", 0 );
            SettingsManager::setSetting( "customGlobalMessageNextLine", 0 );
            }

        for( int i=0; i<numLines; i++ ) {
            delete [] lines[i];
            }
        delete [] lines;
        }
    else {
        // no message, disable
        SettingsManager::setSetting( "customGlobalMessageOn", 0 );
        }
    
    delete [] message;
    }





// sets lastSentMap in inO if chunk goes through
// returns result of send, auto-marks error in inO
int sendMapChunkMessage( LiveObject *inO, 
                         char inDestOverride = false,
                         int inDestOverrideX = 0, 
                         int inDestOverrideY = 0 ) {
    
    if( ! inO->connected ) {
        // act like it was a successful send so we can move on until
        // they reconnect later
        return 1;
        }
    
    int messageLength = 0;

    int xd = inO->xd;
    int yd = inO->yd;
    
    if( inDestOverride ) {
        xd = inDestOverrideX;
        yd = inDestOverrideY;
        }
    
    
    int halfW = chunkDimensionX / 2;
    int halfH = chunkDimensionY / 2;
    
    int fullStartX = xd - halfW;
    int fullStartY = yd - halfH;
    
    int numSent = 0;

    

    if( ! inO->firstMapSent ) {
        // send full rect centered on x,y
        
        inO->firstMapSent = true;
        
        unsigned char *mapChunkMessage = getChunkMessage( fullStartX,
                                                          fullStartY,
                                                          chunkDimensionX,
                                                          chunkDimensionY,
                                                          inO->birthPos,
                                                          &messageLength );
                
        numSent += 
            inO->sock->send( mapChunkMessage, 
                             messageLength, 
                             false, false );
                
        delete [] mapChunkMessage;
        }
    else {
        
        // our closest previous chunk center
        int lastX = inO->lastSentMapX;
        int lastY = inO->lastSentMapY;


        // split next chunk into two bars by subtracting last chunk
        
        int horBarStartX = fullStartX;
        int horBarStartY = fullStartY;
        int horBarW = chunkDimensionX;
        int horBarH = chunkDimensionY;
        
        if( yd > lastY ) {
            // remove bottom of bar
            horBarStartY = lastY + halfH;
            horBarH = yd - lastY;
            }
        else {
            // remove top of bar
            horBarH = lastY - yd;
            }
        

        int vertBarStartX = fullStartX;
        int vertBarStartY = fullStartY;
        int vertBarW = chunkDimensionX;
        int vertBarH = chunkDimensionY;
        
        if( xd > lastX ) {
            // remove left part of bar
            vertBarStartX = lastX + halfW;
            vertBarW = xd - lastX;
            }
        else {
            // remove right part of bar
            vertBarW = lastX - xd;
            }
        
        // now trim vert bar where it intersects with hor bar
        if( yd > lastY ) {
            // remove top of vert bar
            vertBarH -= horBarH;
            }
        else {
            // remove bottom of vert bar
            vertBarStartY = horBarStartY + horBarH;
            vertBarH -= horBarH;
            }
        
        
        // only send if non-zero width and height
        if( horBarW > 0 && horBarH > 0 ) {
            int len;
            unsigned char *mapChunkMessage = getChunkMessage( horBarStartX,
                                                              horBarStartY,
                                                              horBarW,
                                                              horBarH,
                                                              inO->birthPos,
                                                              &len );
            messageLength += len;
            
            numSent += 
                inO->sock->send( mapChunkMessage, 
                                 len, 
                                 false, false );
            
            delete [] mapChunkMessage;
            }
        if( vertBarW > 0 && vertBarH > 0 ) {
            int len;
            unsigned char *mapChunkMessage = getChunkMessage( vertBarStartX,
                                                              vertBarStartY,
                                                              vertBarW,
                                                              vertBarH,
                                                              inO->birthPos,
                                                              &len );
            messageLength += len;
            
            numSent += 
                inO->sock->send( mapChunkMessage, 
                                 len, 
                                 false, false );
            
            delete [] mapChunkMessage;
            }
        }
    
    
    inO->gotPartOfThisFrame = true;
                

    if( numSent == messageLength ) {
        // sent correctly
        inO->lastSentMapX = xd;
        inO->lastSentMapY = yd;
        }
    else {
        setPlayerDisconnected( inO, "Socket write failed" );
        }
    return numSent;
    }







char *getHoldingString( LiveObject *inObject ) {
    
    int holdingID = hideIDForClient( inObject->holdingID );    


    if( inObject->numContained == 0 ) {
        return autoSprintf( "%d", holdingID );
        }

    
    SimpleVector<char> buffer;
    

    char *idString = autoSprintf( "%d", holdingID );
    
    buffer.appendElementString( idString );
    
    delete [] idString;
    
    
    if( inObject->numContained > 0 ) {
        for( int i=0; i<inObject->numContained; i++ ) {
            
            char *idString = autoSprintf( 
                ",%d", 
                hideIDForClient( abs( inObject->containedIDs[i] ) ) );
    
            buffer.appendElementString( idString );
    
            delete [] idString;

            if( inObject->subContainedIDs[i].size() > 0 ) {
                for( int s=0; s<inObject->subContainedIDs[i].size(); s++ ) {
                    
                    idString = autoSprintf( 
                        ":%d", 
                        hideIDForClient( 
                            inObject->subContainedIDs[i].
                            getElementDirect( s ) ) );
    
                    buffer.appendElementString( idString );
                
                    delete [] idString;
                    }
                }
            }
        }
    
    return buffer.getElementString();
    }



// only consider living, non-moving players
char isMapSpotEmptyOfPlayers( int inX, int inY ) {

    int numLive = players.size();
    
    for( int i=0; i<numLive; i++ ) {
        LiveObject *nextPlayer = players.getElement( i );
        
        if( // not about to be deleted
            ! nextPlayer->error &&
            // held players aren't on map (their coordinates are stale)
            ! nextPlayer->heldByOther &&
            // stationary
            nextPlayer->xs == nextPlayer->xd &&
            nextPlayer->ys == nextPlayer->yd &&
            // in this spot
            inX == nextPlayer->xd &&
            inY == nextPlayer->yd ) {
            return false;            
            } 
        }
    
    return true;
    }




// checks both grid of objects and live, non-moving player positions
char isMapSpotEmpty( int inX, int inY, char inConsiderPlayers = true ) {
    int target = getMapObject( inX, inY );
    
    if( target != 0 ) {
        return false;
        }
    
    if( !inConsiderPlayers ) {
        return true;
        }
    
    return isMapSpotEmptyOfPlayers( inX, inY );
    }



static void setFreshEtaDecayForHeld( LiveObject *inPlayer ) {
    
    if( inPlayer->holdingID == 0 ) {
        inPlayer->holdingEtaDecay = 0;
        }
    
    // does newly-held object have a decay defined?
    TransRecord *newDecayT = getMetaTrans( -1, inPlayer->holdingID );
                    
    if( newDecayT != NULL ) {
        inPlayer->holdingEtaDecay = 
            Time::timeSec() + newDecayT->autoDecaySeconds;
        }
    else {
        // no further decay
        inPlayer->holdingEtaDecay = 0;
        }
    }




static void truncateMove( LiveObject *otherPlayer, int blockedStep ) {
    
    int c = computePartialMovePathStep( otherPlayer );
    
    otherPlayer->pathLength
        = blockedStep;
    otherPlayer->pathTruncated
        = true;
    
    // update timing
    double dist = 
        measurePathLength( otherPlayer->xs,
                           otherPlayer->ys,
                           otherPlayer->pathToDest,
                           otherPlayer->pathLength );    
    
    double distAlreadyDone =
        measurePathLength( otherPlayer->xs,
                           otherPlayer->ys,
                           otherPlayer->pathToDest,
                           c );
    
    double moveSpeed = computeMoveSpeed( otherPlayer ) *
        getPathSpeedModifier( otherPlayer->pathToDest,
                              otherPlayer->pathLength );
    
    otherPlayer->moveTotalSeconds 
        = 
        dist / 
        moveSpeed;
    
    double secondsAlreadyDone = 
        distAlreadyDone / 
        moveSpeed;
    
    otherPlayer->moveStartTime = 
        Time::getCurrentTime() - 
        secondsAlreadyDone;
    
    otherPlayer->newMove = true;
    
    otherPlayer->xd 
        = otherPlayer->pathToDest[
            blockedStep - 1].x;
    otherPlayer->yd 
        = otherPlayer->pathToDest[
            blockedStep - 1].y;
    }




static void endAnyMove( LiveObject *nextPlayer ) {
    
    if( nextPlayer->xd != nextPlayer->xs ||
        nextPlayer->yd != nextPlayer->ys ) {
        
        int truncationSpot = 
            computePartialMovePathStep( nextPlayer );
        
        if( truncationSpot < nextPlayer->pathLength - 2 ) {
            
            // truncate a step ahead, to reduce chance 
            // of client-side players needing to turn-around
            // to reach this truncation point
            
            truncateMove( nextPlayer, truncationSpot + 2 );
            }                    
        }
    }

                        


void handleMapChangeToPaths( 
    int inX, int inY, ObjectRecord *inNewObject,
    SimpleVector<int> *inPlayerIndicesToSendUpdatesAbout ) {
    
    if( inNewObject->blocksWalking ) {
    
        GridPos dropSpot = { inX, inY };
          
        int numLive = players.size();
                      
        for( int j=0; j<numLive; j++ ) {
            LiveObject *otherPlayer = 
                players.getElement( j );
            
            if( otherPlayer->error ) {
                continue;
                }

            if( otherPlayer->xd != otherPlayer->xs ||
                otherPlayer->yd != otherPlayer->ys ) {
                
                GridPos cPos = 
                    computePartialMoveSpot( otherPlayer );
                                        
                if( distance( cPos, dropSpot ) 
                    <= 2 * pathDeltaMax ) {
                                            
                    // this is close enough
                    // to this path that it might
                    // block it
                
                    int c = computePartialMovePathStep( otherPlayer );

                    // -1 means starting, pre-path pos is closest
                    // push it up to first path step
                    if( c < 0 ) {
                        c = 0;
                        }

                    char blocked = false;
                    int blockedStep = -1;
                                            
                    for( int p=c; 
                         p<otherPlayer->pathLength;
                         p++ ) {
                                                
                        if( equal( 
                                otherPlayer->
                                pathToDest[p],
                                dropSpot ) ) {
                                                    
                            blocked = true;
                            blockedStep = p;
                            break;
                            }
                        }
                                            
                    if( blocked ) {
                        printf( 
                            "  Blocked by drop\n" );
                        }
                                            

                    if( blocked &&
                        blockedStep > 0 ) {
                        
                        truncateMove( otherPlayer, blockedStep );
                        }
                    else if( blocked ) {
                        // cutting off path
                        // right at the beginning
                        // nothing left

                        // end move now
                        otherPlayer->xd = 
                            otherPlayer->xs;
                                                
                        otherPlayer->yd = 
                            otherPlayer->ys;
                             
                        otherPlayer->posForced = true;
                    
                        inPlayerIndicesToSendUpdatesAbout->push_back( j );
                        }
                    } 
                                        
                }                                    
            }
        }
    
    }



// returns true if found
char findDropSpot( int inX, int inY, int inSourceX, int inSourceY, 
                   GridPos *outSpot ) {

    int barrierRadius = SettingsManager::getIntSetting( "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( "barrierOn", 1 );

    int targetBiome = getMapBiome( inX, inY );
    int targetFloor = getMapFloor( inX, inY );
    
    char found = false;
    int foundX = inX;
    int foundY = inY;
    
    // change direction of throw
    // to match direction of 
    // drop action
    int xDir = inX - inSourceX;
    int yDir = inY - inSourceY;
                                    
        
    if( xDir == 0 && yDir == 0 ) {
        xDir = 1;
        }
    
    // cap to magnitude
    // death drops can be non-adjacent
    if( xDir > 1 ) {
        xDir = 1;
        }
    if( xDir < -1 ) {
        xDir = -1;
        }
    
    if( yDir > 1 ) {
        yDir = 1;
        }
    if( yDir < -1 ) {
        yDir = -1;
        }
        

    // check in y dir first at each
    // expanded radius?
    char yFirst = false;
        
    if( yDir != 0 ) {
        yFirst = true;
        }


    int maxR = 10;


    if( barrierOn ) {
        // don't bother with barrier checks in loop unless we are near
        // barrier edge
        if( barrierOn ) {   
            if( abs( abs( inX ) - barrierRadius ) > maxR + 2 &&
                abs( abs( inY ) - barrierRadius ) > maxR + 2 ) {
                barrierOn = false;
                }
            }
        }
    
        
    for( int d=1; d<maxR && !found; d++ ) {
            
        char doneY0 = false;
            
        for( int yD = -d; yD<=d && !found; 
             yD++ ) {
                
            if( ! doneY0 ) {
                yD = 0;
                }
                
            if( yDir != 0 ) {
                yD *= yDir;
                }
                
            char doneX0 = false;
                
            for( int xD = -d; 
                 xD<=d && !found; 
                 xD++ ) {
                    
                if( ! doneX0 ) {
                    xD = 0;
                    }
                    
                if( xDir != 0 ) {
                    xD *= xDir;
                    }
                    
                    
                if( yD == 0 && xD == 0 ) {
                    if( ! doneX0 ) {
                        doneX0 = true;
                            
                        // back up in loop
                        xD = -d - 1;
                        }
                    continue;
                    }
                                                
                int x = 
                    inSourceX + xD;
                int y = 
                    inSourceY + yD;
                                                
                if( yFirst ) {
                    // swap them
                    // to reverse order
                    // of expansion
                    x = 
                        inSourceX + yD;
                    y =
                        inSourceY + xD;
                    }
                                                


                if( isMapSpotEmpty( x, y ) && 
                    getMapBiome( x, y ) == targetBiome &&
                    getMapFloor( x, y ) == targetFloor ) {
                    
                    found = true;
                    if( barrierOn ) {    
                        if( abs( x ) >= barrierRadius ||
                            abs( y ) >= barrierRadius ) {
                            // outside barrier
                            found = false;
                            }
                        }
                    
                    if( found ) {
                        foundX = x;
                        foundY = y;
                        }
                    }
                                                    
                if( ! doneX0 ) {
                    doneX0 = true;
                                                        
                    // back up in loop
                    xD = -d - 1;
                    }
                }
                                                
            if( ! doneY0 ) {
                doneY0 = true;
                                                
                // back up in loop
                yD = -d - 1;
                }
            }
        }

    outSpot->x = foundX;
    outSpot->y = foundY;
    return found;
    }



#include "spiral.h"

GridPos findClosestEmptyMapSpot( int inX, int inY, int inMaxPointsToCheck,
                                 char *outFound ) {

    int barrierRadius = SettingsManager::getIntSetting( "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( "barrierOn", 1 );


    GridPos center = { inX, inY };

    for( int i=0; i<inMaxPointsToCheck; i++ ) {
        GridPos p = getSpriralPoint( center, i );

        char found = false;
        
        if( isMapSpotEmpty( p.x, p.y, false ) ) {    
            found = true;
            
            if( barrierOn ) {    
                if( abs( p.x ) >= barrierRadius ||
                    abs( p.y ) >= barrierRadius ) {
                    // outside barrier
                    found = false;
                    }
                }
            }
        

        if( found ) {
            *outFound = true;
            return p;
            }
        }
    
    *outFound = false;
    GridPos p = { inX, inY };
    return p;
    }





SimpleVector<ChangePosition> newSpeechPos;

SimpleVector<char*> newSpeechPhrases;
SimpleVector<int> newSpeechPlayerIDs;
SimpleVector<char> newSpeechCurseFlags;



SimpleVector<char*> newLocationSpeech;
SimpleVector<ChangePosition> newLocationSpeechPos;




char *isCurseNamingSay( char *inSaidString );


static void makePlayerSay( LiveObject *inPlayer, char *inToSay ) {    
                        
    if( inPlayer->lastSay != NULL ) {
        delete [] inPlayer->lastSay;
        inPlayer->lastSay = NULL;
        }
    inPlayer->lastSay = stringDuplicate( inToSay );
                        

    char isCurse = false;

    char *cursedName = isCurseNamingSay( inToSay );

    char isYouShortcut = false;
    char isBabyShortcut = false;
    if( strcmp( inToSay, curseYouPhrase ) == 0 ) {
        isYouShortcut = true;
        }
    if( strcmp( inToSay, curseBabyPhrase ) == 0 ) {
        isBabyShortcut = true;
        }

    
    if( inPlayer->isTwin ) {
        // block twins from cursing
        cursedName = NULL;
        
        isYouShortcut = false;
        isBabyShortcut = false;
        }
    
    
    
    if( cursedName != NULL || isYouShortcut ) {

        if( ! SettingsManager::getIntSetting( 
                "allowCrossLineageCursing", 0 ) ) {
            
            // cross-lineage cursing in English forbidden

            int namedPersonLineageEveID = 
                getCurseReceiverLineageEveID( cursedName );
            
            if( namedPersonLineageEveID != inPlayer->lineageEveID ) {
                // We said the curse in plain English, but
                // the named person is not in our lineage
                cursedName = NULL;
                isYouShortcut = false;
                
                // BUT, check if this cursed phrase is correct in 
                // another language below
                }
            }
        }
    

    if( cursedName != NULL ) {
        // it's a pointer into inToSay
        
        // make a copy so we can delete it later
        cursedName = stringDuplicate( cursedName );
        }
    
        
    if( ! inPlayer->isTwin &&
        cursedName == NULL &&
        players.size() >= minActivePlayersForLanguages ) {
        
        // consider cursing in other languages

        int speakerAge = computeAge( inPlayer );
        
        GridPos speakerPos = getPlayerPos( inPlayer );
        
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *otherPlayer = players.getElement( i );
            
            if( otherPlayer == inPlayer ||
                otherPlayer->error ||
                otherPlayer->lineageEveID == inPlayer->lineageEveID ) {
                continue;
                }

            if( distance( speakerPos, getPlayerPos( otherPlayer ) ) >
                getMaxChunkDimension() ) {
                // only consider nearby players
                continue;
                }
                
            char *translatedPhrase =
                mapLanguagePhrase( 
                    inToSay,
                    inPlayer->lineageEveID,
                    otherPlayer->lineageEveID,
                    inPlayer->id,
                    otherPlayer->id,
                    speakerAge,
                    computeAge( otherPlayer ),
                    inPlayer->parentID,
                    otherPlayer->parentID,
                    inPlayer->drunkenness / 10.0 );
            
            cursedName = isCurseNamingSay( translatedPhrase );
            
            if( strcmp( translatedPhrase, curseYouPhrase ) == 0 ) {
                // said CURSE YOU in other language
                isYouShortcut = true;
                }

            // make copy so we can delete later an delete the underlying
            // translatedPhrase now
            
            if( cursedName != NULL ) {
                cursedName = stringDuplicate( cursedName );
                }

            delete [] translatedPhrase;

            if( cursedName != NULL ) {
                int namedPersonLineageEveID = 
                    getCurseReceiverLineageEveID( cursedName );
                
                if( namedPersonLineageEveID == otherPlayer->lineageEveID ) {
                    // the named person belonged to the lineage of the 
                    // person who spoke this language!
                    break;
                    }
                // else cursed in this language, for someone outside
                // this language's line
                delete [] cursedName;
                cursedName = NULL;
                }
            }
        }



    LiveObject *youCursePlayer = NULL;
    LiveObject *babyCursePlayer = NULL;

    if( isYouShortcut ) {
        // find closest player
        GridPos speakerPos = getPlayerPos( inPlayer );
        
        LiveObject *closestOther = NULL;
        double closestDist = 9999999;
        
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *otherPlayer = players.getElement( i );
            
            if( otherPlayer == inPlayer ||
                otherPlayer->error ) {
                continue;
                }
            double dist = distance( speakerPos, getPlayerPos( otherPlayer ) );

            if( dist > getMaxChunkDimension() ) {
                // only consider nearby players
                continue;
                }
            if( dist < closestDist ) {
                closestDist = dist;
                closestOther = otherPlayer;
                }
            }


        if( closestOther != NULL ) {
            youCursePlayer = closestOther;
            
            if( cursedName != NULL ) {
                delete [] cursedName;
                cursedName = NULL;
                }

            if( youCursePlayer->name != NULL ) {
                // allow name-based curse to go through, if possible
                cursedName = stringDuplicate( youCursePlayer->name );
                }
            }
        }
    else if( isBabyShortcut ) {
        LiveObject *youngestOther = NULL;
        double youngestAge = 9999;
        
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *otherPlayer = players.getElement( i );
            
            if( otherPlayer == inPlayer ) {
                // allow error players her, to access recently-dead babies
                continue;
                }
            if( otherPlayer->parentID == inPlayer->id ) {
                double age = computeAge( otherPlayer );
                
                if( age < youngestAge ) {
                    youngestAge = age;
                    youngestOther = otherPlayer;
                    }
                }
            }


        if( youngestOther != NULL ) {
            babyCursePlayer = youngestOther;
            
            if( cursedName != NULL ) {
                delete [] cursedName;
                cursedName = NULL;
                }

            if( babyCursePlayer->name != NULL ) {
                // allow name-based curse to go through, if possible
                cursedName = stringDuplicate( babyCursePlayer->name );
                }
            }
        }


    // make sure, no matter what, we can't curse living 
    // people at a great distance
    // note that, sice we're not tracking dead people here
    // that case will be caught below, since the curses.h tracks death
    // locations
    GridPos speakerPos = getPlayerPos( inPlayer );
    
    if( cursedName != NULL &&
        strcmp( cursedName, "" ) != 0 ) {

        for( int i=0; i<players.size(); i++ ) {
            LiveObject *otherPlayer = players.getElement( i );
            
            if( otherPlayer == inPlayer ||
                otherPlayer->error ) {
                continue;
                }
            if( otherPlayer->name != NULL &&
                strcmp( otherPlayer->name, cursedName ) == 0 ) {
                // matching player
                
                double dist = 
                    distance( speakerPos, getPlayerPos( otherPlayer ) );

                if( dist > getMaxChunkDimension() ) {
                    // too far
                    delete [] cursedName;
                    cursedName = NULL;
                    }
                break;
                }
            }
        }
    
    

    if( cursedName != NULL && 
        strcmp( cursedName, "" ) != 0 ) {
        
        isCurse = cursePlayer( inPlayer->id,
                               inPlayer->lineageEveID,
                               inPlayer->email,
                               speakerPos,
                               getMaxChunkDimension(),
                               cursedName );
        
        if( isCurse ) {
            char *targetEmail = getCurseReceiverEmail( cursedName );
            if( targetEmail != NULL ) {
                setDBCurse( inPlayer->email, targetEmail );
                }
            }
        }
    
    
    if( cursedName != NULL ) {
        delete [] cursedName;
        }
    

    if( !isCurse ) {
        // named curse didn't happen above
        // maybe we used a shortcut, and target didn't have name
        
        if( isYouShortcut && youCursePlayer != NULL &&
            spendCurseToken( inPlayer->email ) ) {
            
            isCurse = true;
            setDBCurse( inPlayer->email, youCursePlayer->email );
            }
        else if( isBabyShortcut && babyCursePlayer != NULL &&
            spendCurseToken( inPlayer->email ) ) {
            
            isCurse = true;
            char *targetEmail = babyCursePlayer->email;
            
            if( strcmp( targetEmail, "email_cleared" ) == 0 ) {
                // deleted players allowed here
                targetEmail = babyCursePlayer->origEmail;
                }
            if( targetEmail != NULL ) {
                setDBCurse( inPlayer->email, targetEmail );
                }
            }
        }


    if( isCurse ) {
        if( ! inPlayer->isTwin && 
            inPlayer->curseStatus.curseLevel == 0 &&
            hasCurseToken( inPlayer->email ) ) {
            inPlayer->curseTokenCount = 1;
            }
        else {
            inPlayer->curseTokenCount = 0;
            }
        inPlayer->curseTokenUpdate = true;
        }

    

    int curseFlag = 0;
    if( isCurse ) {
        curseFlag = 1;
        }
    

    newSpeechPhrases.push_back( stringDuplicate( inToSay ) );
    newSpeechCurseFlags.push_back( curseFlag );
    newSpeechPlayerIDs.push_back( inPlayer->id );

                        
    ChangePosition p = { inPlayer->xd, inPlayer->yd, false };
                        
    // if held, speech happens where held
    if( inPlayer->heldByOther ) {
        LiveObject *holdingPlayer = 
            getLiveObject( inPlayer->heldByOtherID );
                
        if( holdingPlayer != NULL ) {
            p.x = holdingPlayer->xd;
            p.y = holdingPlayer->yd;
            }
        }

    newSpeechPos.push_back( p );



    SimpleVector<int> pipesIn;
    GridPos playerPos = getPlayerPos( inPlayer );
    
    
    if( inPlayer->heldByOther ) {    
        LiveObject *holdingPlayer = 
            getLiveObject( inPlayer->heldByOtherID );
                
        if( holdingPlayer != NULL ) {
            playerPos = getPlayerPos( holdingPlayer );
            }
        }
    
    getSpeechPipesIn( playerPos.x, playerPos.y, &pipesIn );
    
    if( pipesIn.size() > 0 ) {
        for( int p=0; p<pipesIn.size(); p++ ) {
            int pipeIndex = pipesIn.getElementDirect( p );

            SimpleVector<GridPos> *pipesOut = getSpeechPipesOut( pipeIndex );

            for( int i=0; i<pipesOut->size(); i++ ) {
                GridPos outPos = pipesOut->getElementDirect( i );
                
                newLocationSpeech.push_back( stringDuplicate( inToSay ) );
                
                ChangePosition outChangePos = { outPos.x, outPos.y, false };
                newLocationSpeechPos.push_back( outChangePos );
                }
            }
        }
    }


static void forcePlayerToRead( LiveObject *inPlayer,
                               int inObjectID ) {
            
    char metaData[ MAP_METADATA_LENGTH ];
    char found = getMetadata( inObjectID, 
                              (unsigned char*)metaData );

    if( found ) {
        // read what they picked up, subject to limit
                
        unsigned int sayLimit = getSayLimit( inPlayer );
        
        if( computeAge( inPlayer ) < 10 &&
            strlen( metaData ) > sayLimit ) {
            // truncate with ...
            metaData[ sayLimit ] = '.';
            metaData[ sayLimit + 1 ] = '.';
            metaData[ sayLimit + 2 ] = '.';
            metaData[ sayLimit + 3 ] = '\0';
            
            // watch for truncated map metadata
            // trim it off (too young to read maps)
            char *starLoc = strstr( metaData, " *" );
            
            if( starLoc != NULL ) {
                starLoc[0] = '\0';
                }
            }
        char *quotedPhrase = autoSprintf( ":%s", metaData );
        makePlayerSay( inPlayer, quotedPhrase );
        delete [] quotedPhrase;
        }
    }





static void holdingSomethingNew( LiveObject *inPlayer, 
                                 int inOldHoldingID = 0 ) {
    if( inPlayer->holdingID > 0 ) {
       
        ObjectRecord *o = getObject( inPlayer->holdingID );
        
        ObjectRecord *oldO = NULL;
        if( inOldHoldingID > 0 ) {
            oldO = getObject( inOldHoldingID );
            }
        
        if( o->written &&
            ( oldO == NULL ||
              ! ( oldO->written || oldO->writable ) ) ) {

            forcePlayerToRead( inPlayer, inPlayer->holdingID );
            }

        if( o->isFlying ) {
            inPlayer->holdingFlightObject = true;
            }
        else {
            inPlayer->holdingFlightObject = false;
            }
        }
    else {
        inPlayer->holdingFlightObject = false;
        }
    }




static SimpleVector<GraveInfo> newGraves;
static SimpleVector<GraveMoveInfo> newGraveMoves;



static int isGraveSwapDest( int inTargetX, int inTargetY,
                            int inDroppingPlayerID ) {
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( o->error || o->id == inDroppingPlayerID ) {
            continue;
            }
        
        if( o->holdingID > 0 && strstr( getObject( o->holdingID )->description,
                                        "origGrave" ) != NULL ) {
            
            if( inTargetX == o->heldGraveOriginX &&
                inTargetY == o->heldGraveOriginY ) {
                return true;
                }
            }
        }
    
    return false;
    }



// drops an object held by a player at target x,y location
// doesn't check for adjacency (so works for thrown drops too)
// if target spot blocked, will search for empty spot to throw object into
// if inPlayerIndicesToSendUpdatesAbout is NULL, it is ignored
void handleDrop( int inX, int inY, LiveObject *inDroppingPlayer,
                 SimpleVector<int> *inPlayerIndicesToSendUpdatesAbout ) {
    
    int oldHoldingID = inDroppingPlayer->holdingID;
    

    if( oldHoldingID > 0 &&
        getObject( oldHoldingID )->permanent ) {
        // what they are holding is stuck in their
        // hand

        // see if a use-on-bare-ground drop 
        // action applies (example:  dismounting
        // a horse)
                            
        // note that if use on bare ground
        // also has a new actor, that will be lost
        // in this process.
        // (example:  holding a roped lamb when dying,
        //            lamb is dropped, rope is lost)

        TransRecord *bareTrans =
            getPTrans( oldHoldingID, -1 );
                            

        if( bareTrans == NULL ||
            bareTrans->newTarget == 0 ) {
            // no immediate bare ground trans
            // check if there's a timer transition for this held object
            // (like cast fishing pole)
            // and force-run that transition now
            TransRecord *timeTrans = getPTrans( -1, oldHoldingID );
            
            if( timeTrans != NULL && timeTrans->newTarget != 0 ) {
                oldHoldingID = timeTrans->newTarget;
            
                inDroppingPlayer->holdingID = 
                    timeTrans->newTarget;
                holdingSomethingNew( inDroppingPlayer, oldHoldingID );

                setFreshEtaDecayForHeld( inDroppingPlayer );
                }

            if( getObject( oldHoldingID )->permanent ) {
                // still permanent after timed trans
                
                // check again for a bare ground trans
                bareTrans =
                    getPTrans( oldHoldingID, -1 );
                }
            }
        

        if( bareTrans != NULL &&
            bareTrans->newTarget > 0 ) {
            
            if( bareTrans->newActor > 0 ) {
                // something would be left in hand
                
                // throw it down first
                inDroppingPlayer->holdingID = bareTrans->newActor;
                setFreshEtaDecayForHeld( inDroppingPlayer );
                handleDrop( inX, inY, inDroppingPlayer, 
                            inPlayerIndicesToSendUpdatesAbout );
                }

            oldHoldingID = bareTrans->newTarget;
            
            inDroppingPlayer->holdingID = 
                bareTrans->newTarget;
            holdingSomethingNew( inDroppingPlayer, oldHoldingID );

            setFreshEtaDecayForHeld( inDroppingPlayer );
            }
        }
    else if( oldHoldingID > 0 &&
             ! getObject( oldHoldingID )->permanent ) {
        // what they are holding is NOT stuck in their
        // hand

        // see if a use-on-bare-ground drop 
        // action applies (example:  getting wounded while holding a goose)
                            
        // do not consider doing this if use-on-bare-ground leaves something
        // in the hand

        TransRecord *bareTrans =
            getPTrans( oldHoldingID, -1 );
                            
        if( bareTrans != NULL &&
            bareTrans->newTarget > 0 &&
            bareTrans->newActor == 0 ) {
                            
            oldHoldingID = bareTrans->newTarget;
            
            inDroppingPlayer->holdingID = 
                bareTrans->newTarget;
            holdingSomethingNew( inDroppingPlayer, oldHoldingID );

            setFreshEtaDecayForHeld( inDroppingPlayer );
            }
        }

    int targetX = inX;
    int targetY = inY;

    int mapID = getMapObject( inX, inY );
    char mapSpotBlocking = false;
    if( mapID > 0 ) {
        mapSpotBlocking = getObject( mapID )->blocksWalking;
        }
    

    if( ( inDroppingPlayer->holdingID < 0 && mapSpotBlocking )
        ||
        ( inDroppingPlayer->holdingID > 0 && mapID != 0 ) ) {
        
        // drop spot blocked
        // search for another
        // throw held into nearest empty spot
                                    
        
        GridPos spot;

        GridPos playerPos = getPlayerPos( inDroppingPlayer );
        
        char found = findDropSpot( inX, inY, 
                                   playerPos.x, playerPos.y,
                                   &spot );
        
        int foundX = spot.x;
        int foundY = spot.y;



        if( found && inDroppingPlayer->holdingID > 0 ) {
            targetX = foundX;
            targetY = foundY;
            }
        else {
            // no place to drop it, it disappears

            // OR we're holding a baby,
            // then just put the baby where we are
            // (don't ever throw babies, that's weird and exploitable)
            if( inDroppingPlayer->holdingID < 0 ) {
                int babyID = - inDroppingPlayer->holdingID;
                
                LiveObject *babyO = getLiveObject( babyID );
                
                if( babyO != NULL ) {
                    babyO->xd = inDroppingPlayer->xd;
                    babyO->xs = inDroppingPlayer->xd;
                    
                    babyO->yd = inDroppingPlayer->yd;
                    babyO->ys = inDroppingPlayer->yd;

                    babyO->heldByOther = false;

                    if( isFertileAge( inDroppingPlayer ) ) {    
                        // reset food decrement time
                        babyO->foodDecrementETASeconds =
                            Time::getCurrentTime() +
                            computeFoodDecrementTimeSeconds( babyO );
                        }
                    
                    if( inPlayerIndicesToSendUpdatesAbout != NULL ) {    
                        inPlayerIndicesToSendUpdatesAbout->push_back( 
                            getLiveObjectIndex( babyID ) );
                        }
                    
                    }
                
                }
            
            inDroppingPlayer->holdingID = 0;
            inDroppingPlayer->holdingEtaDecay = 0;
            inDroppingPlayer->heldOriginValid = 0;
            inDroppingPlayer->heldOriginX = 0;
            inDroppingPlayer->heldOriginY = 0;
            inDroppingPlayer->heldTransitionSourceID = -1;
            
            if( inDroppingPlayer->numContained != 0 ) {
                clearPlayerHeldContained( inDroppingPlayer );
                }
            return;
            }            
        }
    
    
    if( inDroppingPlayer->holdingID < 0 ) {
        // dropping a baby
        
        int babyID = - inDroppingPlayer->holdingID;
                
        LiveObject *babyO = getLiveObject( babyID );
        
        if( babyO != NULL ) {
            babyO->xd = targetX;
            babyO->xs = targetX;
                    
            babyO->yd = targetY;
            babyO->ys = targetY;
            
            babyO->heldByOther = false;
            
            // force baby pos
            // baby can wriggle out of arms in same server step that it was
            // picked up.  In that case, the clients will never get the
            // message that the baby was picked up.  The baby client could
            // be in the middle of a client-side move, and we need to force
            // them back to their true position.
            babyO->posForced = true;
            
            if( isFertileAge( inDroppingPlayer ) ) {    
                // reset food decrement time
                babyO->foodDecrementETASeconds =
                    Time::getCurrentTime() +
                    computeFoodDecrementTimeSeconds( babyO );
                }

            if( inPlayerIndicesToSendUpdatesAbout != NULL ) {
                inPlayerIndicesToSendUpdatesAbout->push_back( 
                    getLiveObjectIndex( babyID ) );
                }
            }
        
        inDroppingPlayer->holdingID = 0;
        inDroppingPlayer->holdingEtaDecay = 0;
        inDroppingPlayer->heldOriginValid = 0;
        inDroppingPlayer->heldOriginX = 0;
        inDroppingPlayer->heldOriginY = 0;
        inDroppingPlayer->heldTransitionSourceID = -1;
        
        return;
        }
    
    setResponsiblePlayer( inDroppingPlayer->id );
    
    ObjectRecord *o = getObject( inDroppingPlayer->holdingID );
                                
    if( strstr( o->description, "origGrave" ) 
        != NULL ) {
                                    
        setGravePlayerID( 
            targetX, targetY, inDroppingPlayer->heldGravePlayerID );
        
        int swapDest = isGraveSwapDest( targetX, targetY, 
                                        inDroppingPlayer->id );
        
        // see if another player has target location in air


        GraveMoveInfo g = { 
            { inDroppingPlayer->heldGraveOriginX,
              inDroppingPlayer->heldGraveOriginY },
            { targetX,
              targetY },
            swapDest };
        newGraveMoves.push_back( g );
        }


    setMapObject( targetX, targetY, inDroppingPlayer->holdingID );
    setEtaDecay( targetX, targetY, inDroppingPlayer->holdingEtaDecay );

    transferHeldContainedToMap( inDroppingPlayer, targetX, targetY );
    
                                

    setResponsiblePlayer( -1 );
                                
    inDroppingPlayer->holdingID = 0;
    inDroppingPlayer->holdingEtaDecay = 0;
    inDroppingPlayer->heldOriginValid = 0;
    inDroppingPlayer->heldOriginX = 0;
    inDroppingPlayer->heldOriginY = 0;
    inDroppingPlayer->heldTransitionSourceID = -1;
                                
    // watch out for truncations of in-progress
    // moves of other players
            
    ObjectRecord *droppedObject = getObject( oldHoldingID );
   
    if( inPlayerIndicesToSendUpdatesAbout != NULL ) {    
        handleMapChangeToPaths( targetX, targetY, droppedObject,
                                inPlayerIndicesToSendUpdatesAbout );
        }
    
    
    }



LiveObject *getAdultHolding( LiveObject *inBabyObject ) {
    int numLive = players.size();
    
    for( int j=0; j<numLive; j++ ) {
        LiveObject *adultO = players.getElement( j );

        if( - adultO->holdingID == inBabyObject->id ) {
            return adultO;
            }
        }
    return NULL;
    }



void handleForcedBabyDrop( 
    LiveObject *inBabyObject,
    SimpleVector<int> *inPlayerIndicesToSendUpdatesAbout ) {
    
    int numLive = players.size();
    
    for( int j=0; j<numLive; j++ ) {
        LiveObject *adultO = players.getElement( j );

        if( - adultO->holdingID == inBabyObject->id ) {

            // don't need to send update about this adult's
            // holding status.
            // the update sent about the baby will inform clients
            // that the baby is no longer held by this adult
            //inPlayerIndicesToSendUpdatesAbout->push_back( j );
            
            GridPos dropPos;
            
            if( adultO->xd == 
                adultO->xs &&
                adultO->yd ==
                adultO->ys ) {
                
                dropPos.x = adultO->xd;
                dropPos.y = adultO->yd;
                }
            else {
                dropPos = 
                    computePartialMoveSpot( adultO );
                }
            
            
            handleDrop( 
                dropPos.x, dropPos.y, 
                adultO,
                inPlayerIndicesToSendUpdatesAbout );

            
            break;
            }
        }
    }



static void handleHoldingChange( LiveObject *inPlayer, int inNewHeldID );



static void swapHeldWithGround( 
    LiveObject *inPlayer, int inTargetID, 
    int inMapX, int inMapY,
    SimpleVector<int> *inPlayerIndicesToSendUpdatesAbout) {
    
    
    if( inTargetID == inPlayer->holdingID &&
        inPlayer->numContained == 0 &&
        getNumContained( inMapX, inMapY ) == 0 ) {
        // swap of same non-container object with self
        // ignore this, to prevent weird case of swapping
        // grave basket with self
        return;
        }
    

    timeSec_t newHoldingEtaDecay = getEtaDecay( inMapX, inMapY );
    
    FullMapContained f = getFullMapContained( inMapX, inMapY );


    int gravePlayerID = getGravePlayerID( inMapX, inMapY );
        
    if( gravePlayerID > 0 ) {
            
        // player action actually picked up this grave
        
        // clear it from ground
        setGravePlayerID( inMapX, inMapY, 0 );
        }

    
    clearAllContained( inMapX, inMapY );
    setMapObject( inMapX, inMapY, 0 );
    
    handleDrop( inMapX, inMapY, inPlayer, inPlayerIndicesToSendUpdatesAbout );
    
    
    inPlayer->holdingID = inTargetID;
    inPlayer->holdingEtaDecay = newHoldingEtaDecay;
    
    setContained( inPlayer, f );


    // does bare-hand action apply to this newly-held object
    // one that results in something new in the hand and
    // nothing on the ground?
    
    // if so, it is a pick-up action, and it should apply here
    
    TransRecord *pickupTrans = getPTrans( 0, inTargetID );

    char newHandled = false;
                
    if( pickupTrans != NULL && pickupTrans->newActor > 0 &&
        pickupTrans->newTarget == 0 ) {
                    
        int newTargetID = pickupTrans->newActor;
        
        if( newTargetID != inTargetID ) {
            handleHoldingChange( inPlayer, newTargetID );
            newHandled = true;
            }
        }
    
    if( ! newHandled ) {
        holdingSomethingNew( inPlayer );
        }
    
    inPlayer->heldOriginValid = 1;
    inPlayer->heldOriginX = inMapX;
    inPlayer->heldOriginY = inMapY;
    inPlayer->heldTransitionSourceID = -1;


    inPlayer->heldGravePlayerID = 0;

    if( inPlayer->holdingID > 0 &&
        strstr( getObject( inPlayer->holdingID )->description, 
                "origGrave" ) != NULL &&
        gravePlayerID > 0 ) {
    
        inPlayer->heldGraveOriginX = inMapX;
        inPlayer->heldGraveOriginY = inMapY;
        inPlayer->heldGravePlayerID = gravePlayerID;
        }
    }









// returns 0 for NULL
static int objectRecordToID( ObjectRecord *inRecord ) {
    if( inRecord == NULL ) {
        return 0;
        }
    else {
        return inRecord->id;
        }
    }



typedef struct UpdateRecord{
        char *formatString;
        char posUsed;
        int absolutePosX, absolutePosY;
        GridPos absoluteActionTarget;
        int absoluteHeldOriginX, absoluteHeldOriginY;
    } UpdateRecord;



static char *getUpdateLineFromRecord( 
    UpdateRecord *inRecord, GridPos inRelativeToPos, GridPos inObserverPos ) {
    
    if( inRecord->posUsed ) {
        
        GridPos updatePos = { inRecord->absolutePosX, inRecord->absolutePosY };
        
        if( distance( updatePos, inObserverPos ) > 
            getMaxChunkDimension() * 2 ) {
            
            // this update is for a far-away player
            
            // put dummy positions in to hide their coordinates
            // so that people sniffing the protocol can't get relative
            // location information
            
            return autoSprintf( inRecord->formatString,
                                1977, 1977,
                                1977, 1977,
                                1977, 1977 );
            }


        return autoSprintf( inRecord->formatString,
                            inRecord->absoluteActionTarget.x 
                            - inRelativeToPos.x,
                            inRecord->absoluteActionTarget.y 
                            - inRelativeToPos.y,
                            inRecord->absoluteHeldOriginX - inRelativeToPos.x, 
                            inRecord->absoluteHeldOriginY - inRelativeToPos.y,
                            inRecord->absolutePosX - inRelativeToPos.x, 
                            inRecord->absolutePosY - inRelativeToPos.y );
        }
    else {
        // posUsed false only if thise is a DELETE PU message
        // set all positions to 0 in that case
        return autoSprintf( inRecord->formatString,
                            0, 0,
                            0, 0 );
        }
    }



static char isYummy( LiveObject *inPlayer, int inObjectID ) {
    ObjectRecord *o = getObject( inObjectID );
    
    if( o->isUseDummy ) {
        inObjectID = o->useDummyParent;
        o = getObject( inObjectID );
        }

    if( o->foodValue == 0 ) {
        return false;
        }

    for( int i=0; i<inPlayer->yummyFoodChain.size(); i++ ) {
        if( inObjectID == inPlayer->yummyFoodChain.getElementDirect(i) ) {
            return false;
            }
        }
    return true;
    }



static void updateYum( LiveObject *inPlayer, int inFoodEatenID,
                       char inFedSelf = true ) {

    char wasYummy = true;
    
    if( ! isYummy( inPlayer, inFoodEatenID ) ) {
        wasYummy = false;
        
        // chain broken
        
        // only feeding self can break chain
        if( inFedSelf ) {
            inPlayer->yummyFoodChain.deleteAll();
            }
        }
    
    
    ObjectRecord *o = getObject( inFoodEatenID );
    
    if( o->isUseDummy ) {
        inFoodEatenID = o->useDummyParent;
        }
    
    
    // add to chain
    // might be starting a new chain
    // (do this if fed yummy food by other player too)
    if( wasYummy ||
        inPlayer->yummyFoodChain.size() == 0 ) {
        
        inPlayer->yummyFoodChain.push_back( inFoodEatenID );
        }
    

    int currentBonus = inPlayer->yummyFoodChain.size() - 1;

    if( currentBonus < 0 ) {
        currentBonus = 0;
        }    

    if( wasYummy ) {
        // only get bonus if actually was yummy (whether fed self or not)
        // chain not broken if fed non-yummy by other, but don't get bonus
        
        // apply foodScaleFactor here to scale value of YUM along with
        // the global scale of other foods.
        
        inPlayer->yummyBonusStore += 
            lrint( foodScaleFactor * currentBonus );
        }
    
    }



static char canPlayerUseTool( LiveObject *inPlayer, int inToolID ) {
    ObjectRecord *toolO = getObject( inToolID );
                                    
    // is it a marked tool?
    int toolSet = toolO->toolSetIndex;
    
    if( toolSet != -1 &&
        inPlayer->learnedTools.getElementIndex( toolSet ) == -1 ) {
        // not in player's learned tool set
        return false;
        }
    
    return true;
    }



static UpdateRecord getUpdateRecord( 
    LiveObject *inPlayer,
    char inDelete,
    char inPartial = false ) {

    char *holdingString = getHoldingString( inPlayer );
    
    // this is 0 if still in motion (mid-move update)
    int doneMoving = 0;
    
    if( inPlayer->xs == inPlayer->xd &&
        inPlayer->ys == inPlayer->yd &&
        ! inPlayer->heldByOther ) {
        // not moving
        doneMoving = inPlayer->lastMoveSequenceNumber;
        }
    
    char midMove = false;
    
    if( inPartial || 
        inPlayer->xs != inPlayer->xd ||
        inPlayer->ys != inPlayer->yd ) {
        
        midMove = true;
        }
    

    UpdateRecord r;
        

    char *posString;
    if( inDelete ) {
        posString = stringDuplicate( "0 0 X X" );
        r.posUsed = false;
        }
    else {
        int x, y;

        r.posUsed = true;

        if( doneMoving > 0 || ! midMove ) {
            x = inPlayer->xs;
            y = inPlayer->ys;
            }
        else {
            // mid-move, and partial position requested
            GridPos p = computePartialMoveSpot( inPlayer );
            
            x = p.x;
            y = p.y;
            }
        
        posString = autoSprintf( "%d %d %%d %%d",          
                                 doneMoving,
                                 inPlayer->posForced );
        r.absolutePosX = x;
        r.absolutePosY = y;
        }
    
    SimpleVector<char> clothingListBuffer;
    
    for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
        ObjectRecord *cObj = clothingByIndex( inPlayer->clothing, c );
        int id = 0;
        
        if( cObj != NULL ) {
            id = objectRecordToID( cObj );
            }
        
        char *idString = autoSprintf( "%d", hideIDForClient( id ) );
        
        clothingListBuffer.appendElementString( idString );
        delete [] idString;
        
        if( cObj != NULL && cObj->numSlots > 0 ) {
            
            for( int cc=0; cc<inPlayer->clothingContained[c].size(); cc++ ) {
                char *contString = 
                    autoSprintf( 
                        ",%d", 
                        hideIDForClient( 
                            inPlayer->
                            clothingContained[c].getElementDirect( cc ) ) );
                
                clothingListBuffer.appendElementString( contString );
                delete [] contString;
                }
            }

        if( c < NUM_CLOTHING_PIECES - 1 ) {
            clothingListBuffer.push_back( ';' );
            }
        }
    
    char *clothingList = clothingListBuffer.getElementString();


    char *deathReason;
    
    if( inDelete && inPlayer->deathReason != NULL ) {
        deathReason = stringDuplicate( inPlayer->deathReason );
        }
    else {
        deathReason = stringDuplicate( "" );
        }
    
    
    int heldYum = 0;
    
    if( inPlayer->holdingID > 0 &&
        isYummy( inPlayer, inPlayer->holdingID ) ) {
        heldYum = 1;
        }

    int heldLearned = 0;
    
    if( inPlayer->holdingID > 0 &&
        canPlayerUseTool( inPlayer, inPlayer->holdingID ) ) {
        heldLearned = 1;
        }
        

    r.formatString = autoSprintf( 
        "%d %d %d %d %%d %%d %s %d %%d %%d %d "
        "%.2f %s %.2f %.2f %.2f %s %d %d %d %d %d%s\n",
        inPlayer->id,
        inPlayer->displayID,
        inPlayer->facingOverride,
        inPlayer->actionAttempt,
        //inPlayer->actionTarget.x - inRelativeToPos.x,
        //inPlayer->actionTarget.y - inRelativeToPos.y,
        holdingString,
        inPlayer->heldOriginValid,
        //inPlayer->heldOriginX - inRelativeToPos.x,
        //inPlayer->heldOriginY - inRelativeToPos.y,
        hideIDForClient( inPlayer->heldTransitionSourceID ),
        inPlayer->heat,
        posString,
        computeAge( inPlayer ),
        1.0 / getAgeRate(),
        computeMoveSpeed( inPlayer ),
        clothingList,
        inPlayer->justAte,
        hideIDForClient( inPlayer->justAteID ),
        inPlayer->responsiblePlayerID,
        heldYum,
        heldLearned,
        deathReason );
    
    delete [] deathReason;
    

    r.absoluteActionTarget = inPlayer->actionTarget;
    
    if( inPlayer->heldOriginValid ) {
        r.absoluteHeldOriginX = inPlayer->heldOriginX;
        r.absoluteHeldOriginY = inPlayer->heldOriginY;
        }
    else {
        // we set 0,0 to clear held origins in many places in the code
        // if we leave that as an absolute pos, our birth pos leaks through
        // when we make it birth-pos relative
        
        // instead, substitute our birth pos for all invalid held pos coords
        // to prevent this
        r.absoluteHeldOriginX = inPlayer->birthPos.x;
        r.absoluteHeldOriginY = inPlayer->birthPos.y;
        }
    
        

    inPlayer->justAte = false;
    inPlayer->justAteID = 0;
    
    // held origin only valid once
    inPlayer->heldOriginValid = 0;
    
    inPlayer->facingOverride = 0;
    inPlayer->actionAttempt = 0;

    delete [] holdingString;
    delete [] posString;
    delete [] clothingList;
    
    return r;
    }



// inDelete true to send X X for position
// inPartial gets update line for player's current possition mid-path
// positions in update line will be relative to inRelativeToPos
static char *getUpdateLine( LiveObject *inPlayer, GridPos inRelativeToPos,
                            GridPos inObserverPos,
                            char inDelete,
                            char inPartial = false ) {
    
    UpdateRecord r = getUpdateRecord( inPlayer, inDelete, inPartial );
    
    char *line = getUpdateLineFromRecord( &r, inRelativeToPos, inObserverPos );

    delete [] r.formatString;
    
    return line;
    }




// if inTargetID set, we only detect whether inTargetID is close enough to
// be hit
// otherwise, we find the lowest-id player that is hit and return that
static LiveObject *getHitPlayer( int inX, int inY,
                                 int inTargetID = -1,
                                 char inCountMidPath = false,
                                 int inMaxAge = -1,
                                 int inMinAge = -1,
                                 int *outHitIndex = NULL ) {
    GridPos targetPos = { inX, inY };

    int numLive = players.size();
                                    
    LiveObject *hitPlayer = NULL;
                                    
    for( int j=0; j<numLive; j++ ) {
        LiveObject *otherPlayer = 
            players.getElement( j );
        
        if( otherPlayer->error ) {
            continue;
            }
        
        if( otherPlayer->heldByOther ) {
            // ghost position of a held baby
            continue;
            }
        
        if( inMaxAge != -1 &&
            computeAge( otherPlayer ) > inMaxAge ) {
            continue;
            }

        if( inMinAge != -1 &&
            computeAge( otherPlayer ) < inMinAge ) {
            continue;
            }
        
        if( inTargetID != -1 &&
            otherPlayer->id != inTargetID ) {
            continue;
            }

        if( otherPlayer->xd == 
            otherPlayer->xs &&
            otherPlayer->yd ==
            otherPlayer->ys ) {
            // other player standing still
                                            
            if( otherPlayer->xd ==
                inX &&
                otherPlayer->yd ==
                inY ) {
                                                
                // hit
                hitPlayer = otherPlayer;
                if( outHitIndex != NULL ) {
                    *outHitIndex = j;
                    }
                break;
                }
            }
        else {
            // other player moving
                
            GridPos cPos = 
                computePartialMoveSpot( 
                    otherPlayer );
                                        
            if( equal( cPos, targetPos ) ) {
                // hit
                hitPlayer = otherPlayer;
                if( outHitIndex != NULL ) {
                    *outHitIndex = j;
                    }
                break;
                }
            else if( inCountMidPath ) {
                
                int c = computePartialMovePathStep( otherPlayer );

                // consider path step before and after current location
                for( int i=-1; i<=1; i++ ) {
                    int testC = c + i;
                    
                    if( testC >= 0 && testC < otherPlayer->pathLength ) {
                        cPos = otherPlayer->pathToDest[testC];
                 
                        if( equal( cPos, targetPos ) ) {
                            // hit
                            hitPlayer = otherPlayer;
                            if( outHitIndex != NULL ) {
                                *outHitIndex = j;
                                }
                            break;
                            }
                        }
                    }
                if( hitPlayer != NULL ) {
                    break;
                    }
                }
            }
        }

    return hitPlayer;
    }



static int isPlayerCountable( LiveObject *p, int inLineageEveID = -1 ) {
    if( p->error ) {
        return false;
        }
    if( p->isTutorial ) {
        return false;
        }
    if( p->curseStatus.curseLevel > 0 ) {
        return false;
        }
    if( p->vogMode ) {
        return false;
        }
    
    if( inLineageEveID != -1 &&
        p->lineageEveID != inLineageEveID ) {
        return false;
        }
    return true;
    }



// if inLineageEveID != -1, it specifies that we count fertile mothers
// ONLY in that family
static int countFertileMothers( int inLineageEveID = -1 ) {
    
    int barrierRadius = 
        SettingsManager::getIntSetting( 
            "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( 
        "barrierOn", 1 );
    
    int c = 0;
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *p = players.getElement( i );
        
        if( ! isPlayerCountable( p, inLineageEveID ) ) {
            continue;
            }

        if( isFertileAge( p ) ) {
            if( barrierOn ) {
                // only fertile mothers inside the barrier
                GridPos pos = getPlayerPos( p );
                
                if( abs( pos.x ) < barrierRadius &&
                    abs( pos.y ) < barrierRadius ) {
                    c++;
                    }
                }
            else {
                c++;
                }
            }
        }
    
    return c;
    }



// girls are females who are not fertile yet, but will be
// if inLineageEveID != -1, it specifies that we count girls
// ONLY in that family
static int countGirls( int inLineageEveID = -1 ) {
    
    int barrierRadius = 
        SettingsManager::getIntSetting( 
            "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( 
        "barrierOn", 1 );
    
    int c = 0;
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *p = players.getElement( i );

        if( ! isPlayerCountable( p, inLineageEveID ) ) {
            continue;
            }
        
        if( getFemale( p ) && computeAge( p ) < getFirstFertileAge() ) {
            if( barrierOn ) {
                // only girls inside the barrier
                GridPos pos = getPlayerPos( p );
                
                if( abs( pos.x ) < barrierRadius &&
                    abs( pos.y ) < barrierRadius ) {
                    c++;
                    }
                }
            else {
                c++;
                }
            }
        }
    
    return c;
    }



static int countHelplessBabies() {
    
    int barrierRadius = 
        SettingsManager::getIntSetting( 
            "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( 
        "barrierOn", 1 );
    
    int c = 0;
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *p = players.getElement( i );

        if( ! isPlayerCountable( p ) ) {
            continue;
            }

        if( computeAge( p ) < defaultActionAge ) {
            if( barrierOn ) {
                // only babies inside the barrier
                GridPos pos = getPlayerPos( p );
                
                if( abs( pos.x ) < barrierRadius &&
                    abs( pos.y ) < barrierRadius ) {
                    c++;
                    }
                }
            else {
                c++;
                }
            }
        }
    
    return c;
    }



// counts only those inside barrier, if barrier on
// always ignores tutorial and donkytown players
static int countLivingPlayers() {
    
    int barrierRadius = 
        SettingsManager::getIntSetting( 
            "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( 
        "barrierOn", 1 );
    
    int c = 0;
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *p = players.getElement( i );

        if( ! isPlayerCountable( p ) ) {
            continue;
            }

        if( barrierOn ) {
            // only people inside the barrier
            GridPos pos = getPlayerPos( p );
            
            if( abs( pos.x ) < barrierRadius &&
                abs( pos.y ) < barrierRadius ) {
                c++;
                }
            }
        else {
            c++;
            }
        }
    
    return c;
    }




static int countFamilies() {
    
    int barrierRadius = 
        SettingsManager::getIntSetting( 
            "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( 
        "barrierOn", 1 );
    
    SimpleVector<int> uniqueLines;

    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *p = players.getElement( i );

        if( ! isPlayerCountable( p ) ) {
            continue;
            }

        int lineageEveID = p->lineageEveID;
            
        if( uniqueLines.getElementIndex( lineageEveID ) == -1 ) {
            
            if( barrierOn ) {
                // only those inside the barrier
                GridPos pos = getPlayerPos( p );
                
                if( abs( pos.x ) < barrierRadius &&
                    abs( pos.y ) < barrierRadius ) {
                    uniqueLines.push_back( lineageEveID );
                    }
                }
            else {
                uniqueLines.push_back( lineageEveID );
                }
            }
        }
    
    return uniqueLines.size();
    }



// make sure same family isn't picked too often to get a baby
// don't hammer Eve even though her fam is currently the weakest
typedef struct FamilyPickedRecord {
        int lineageEveID;
        double lastPickTime;
    } FamilyPickedRecord;


static SimpleVector<FamilyPickedRecord> familiesRecentlyPicked;


// let one mom wait 1.5 minutes between BB
static double waitSecondsPerMom = 1.5 * 60.0;


static char isFamilyTooRecent( int inLineageEveID, int inMomCount ) {
    double curTime = Time::getCurrentTime();
    
    
    double waitTime = waitSecondsPerMom / inMomCount;
    

    for( int i=0; i<familiesRecentlyPicked.size(); i++ ) {
        FamilyPickedRecord *r = familiesRecentlyPicked.getElement( i );
        if( r->lineageEveID == inLineageEveID ) {
            if( curTime - r->lastPickTime < waitTime ) {
                // fam got BB too recently
                return true;
                }
            else {
                return false;
                }
            }
        }
    
    return false;
    }



static void markFamilyGotBabyNow( int inLineageEveID ) {
    double curTime = Time::getCurrentTime();
    
    for( int i=0; i<familiesRecentlyPicked.size(); i++ ) {
        FamilyPickedRecord *r = familiesRecentlyPicked.getElement( i );
        if( r->lineageEveID == inLineageEveID ) {
            r->lastPickTime = curTime;
            return;
            }
        }
    
    // not found
    FamilyPickedRecord r = { inLineageEveID, curTime };
    familiesRecentlyPicked.push_back( r );
    }




static int getNextBabyFamilyLineageEveIDFewestFemales() {
    SimpleVector<int> uniqueFams;
    
    int minFemales = 999999;
    int minFemalesLineageEveID = -1;

    for( int i=0; i<players.size(); i++ ) {
        LiveObject *p = players.getElement( i );

        if( ! isPlayerCountable( p ) ) {
            continue;
            }
        if( uniqueFams.getElementIndex( p->lineageEveID ) == -1 ) {
            uniqueFams.push_back( p->lineageEveID );
            }
        }
    


    // clear stale family records
    for( int i=0; i<familiesRecentlyPicked.size(); i++ ) {
        FamilyPickedRecord *r = familiesRecentlyPicked.getElement( i );
        
        if( uniqueFams.getElementIndex( r->lineageEveID ) == -1 ) {
            // stale
            familiesRecentlyPicked.deleteElement( i );
            i--;
            }
        }
    

    waitSecondsPerMom = 
        SettingsManager::getDoubleSetting(
            "weakFamilyPickWaitSecondsPerMom", 1.5 * 3600 );
    

    for( int i=0; i<uniqueFams.size(); i++ ) {
        int lineageEveID = 
            uniqueFams.getElementDirect( i );

        int famMothers = countFertileMothers( lineageEveID );

        if( isFamilyTooRecent( lineageEveID, famMothers ) ) {
            continue;
            }


        int famGirls = countGirls( lineageEveID );
        
        int famFemales = famMothers + famGirls;
        
        if( famMothers > 0 ) {
            // don't pick a fam that has no mothers at all
            // there's no point (bb can't be born there now)
            
            if( famFemales < minFemales ) {
                minFemales = famFemales;
                minFemalesLineageEveID = lineageEveID;
                }
            }
        }

    if( minFemalesLineageEveID != -1 ) {
        markFamilyGotBabyNow( minFemalesLineageEveID );
        }
    
    return minFemalesLineageEveID;
    }



// allows us to switch back and forth between methods 
static int getNextBabyFamilyLineageEveID() {
    if( false ) {
        // suppress unused warning
        return getNextBabyFamilyLineageEveIDRoundRobin();
        }
    return getNextBabyFamilyLineageEveIDFewestFemales();
    }



static char isEveWindow() {
    
    if( players.size() <=
        SettingsManager::getIntSetting( "minActivePlayersForEveWindow", 15 ) ) {
        // not enough players
        // always Eve window
        
        // new window starts if we ever get enough players again
        eveWindowStart = 0;
        eveWindowOver = false;
        
        return true;
        }

    if( eveWindowStart == 0 ) {
        // start window now
        eveWindowStart = Time::getCurrentTime();
        eveWindowOver = false;
        return true;
        }
    else {
        double secSinceStart = Time::getCurrentTime() - eveWindowStart;
        
        if( secSinceStart >
            SettingsManager::getIntSetting( "eveWindowSeconds", 3600 ) ) {
            
            if( ! eveWindowOver ) {
                // eveWindow just ended

                restockPostWindowFamilies();
                }
            
            eveWindowOver = true;
            return false;
            }

        eveWindowOver = false;
        return true;
        }
    }



static void triggerApocalypseNow( const char *inMessage ) {
    AppLog::infoF( "Local apocalypse triggered:  %s\n", inMessage );
    
    apocalypseTriggered = true;
    }



static void setupToolSlots( LiveObject *inPlayer ) {
    int min = SettingsManager::getIntSetting( "baseToolSlotsPerPlayer", 6 );
    int max = SettingsManager::getIntSetting( "maxToolSlotsPerPlayer", 12 );
    
    int minActive = 
        SettingsManager::getIntSetting( "minActivePlayersForToolSlots", 15 );
    
    if( countLivingPlayers() < minActive ) {
        // low-pop players know all tools
        getAllToolSets( &( inPlayer->learnedTools ) );
        
        // slots don't matter
        inPlayer->numToolSlots = min;
        
        return;
        }
    


    // this sigmoid function found in gnuplot which looks good
    // for 10 and 5

    // plot 15 / (1 + 1.03**-(x - 100)) + 4.258
    
    // however, actual range here is 19.258 and 5

    // need to compute these parameters based on max and min
    
    // plot A / (1 + C**-(x - D)) + B
    
    double C = 1.03;
    double D = 100;

    double A = max - min;
    
    double valAtZero = A / ( 1 + pow( C, D ) );
    
    double B = min - valAtZero;
    
    
    // when this is called, we already have a valid fitness score (or 0)
    // can be negative or positive, with no limits

    
    int slots = 
        lrint( 
            A / ( 1 + pow( C, -( inPlayer->fitnessScore - D ) ) ) + B );
    
    if( inPlayer->fitnessScore < 0 ) {
        // score is negative?  Auto-ding them one slot when this happens
        slots -= 1;

        // otherwise, the above formulat likely doesn't have a transition
        // around 0
        }
    

    
    // no negative slots
    if( slots < 0 ) {
        slots = 0;
        }


    const char *slotWord = "SLOTS";
        
    if( abs( slots - min ) == 1 ) {
        slotWord = "SLOT";
        }


    const char *slotTotalWord = "SLOTS";
        
    if( slots == 1 ) {
        slotTotalWord = "SLOT";
        }
    
    char *message = autoSprintf( "YOUR GENETIC FITNESS SCORE IS %.1lf**"
                                 "YOU GET %d BONUS TOOL %s, "
                                 "FOR A TOTAL OF %d %s.",
                                 inPlayer->fitnessScore,
                                 slots - min, slotWord, 
                                 slots, slotTotalWord );
    
    sendGlobalMessage( message, inPlayer );
    
    delete [] message;
    

    inPlayer->numToolSlots = slots;

    if( inPlayer->isTutorial && inPlayer->learnedTools.size() == 0 ) {
        // tutorial players know all tools
        getAllToolSets( &( inPlayer->learnedTools ) );
        }
    }


typedef struct ForceSpawnRecord {
        GridPos pos;
        double age;
        char *firstName;
        char *lastName;
        int displayID;
        int hatID;
        int tunicID;
        int bottomID;
        int frontShoeID;
        int backShoeID;
    } ForceSpawnRecord;



// strings in outRecordToFill destroyed by caller
char getForceSpawn( char *inEmail, ForceSpawnRecord *outRecordToFill ) {
    char *cont = SettingsManager::getSettingContents( "forceSpawnAccounts" );
    
    if( cont == NULL ) {
        return false;
        }
    int numParts;
    char **lines = split( cont, "\n", &numParts );

    delete [] cont;
    
    char found = false;

    for( int i=0; i<numParts; i++ ) {
        
        if( strstr( lines[i], inEmail ) == lines[i] ) {
            // matches email

            char emailBuff[100];
            
            int on = 0;
            
            sscanf( lines[i],
                    "%99s %d", emailBuff, &on );

            if( on == 1 ) {
                
                outRecordToFill->firstName = new char[20];
                outRecordToFill->lastName = new char[20];
                

                int numRead = sscanf( 
                    lines[i],
                    "%99s %d %d,%d %lf %19s %19s %d %d %d %d %d %d", 
                    emailBuff, &on,
                    &outRecordToFill->pos.x,
                    &outRecordToFill->pos.y,
                    &outRecordToFill->age,
                    outRecordToFill->firstName,
                    outRecordToFill->lastName,
                    &outRecordToFill->displayID,
                    &outRecordToFill->hatID,
                    &outRecordToFill->tunicID,
                    &outRecordToFill->bottomID,
                    &outRecordToFill->frontShoeID,
                    &outRecordToFill->backShoeID );
                
                if( numRead == 13 ) {
                    found = true;
                    }
                else {
                    delete [] outRecordToFill->firstName;
                    delete [] outRecordToFill->lastName;
                    }
                }
            break;
            }
        }


    for( int i=0; i<numParts; i++ ) {
        delete [] lines[i];
        }
    delete [] lines;
    
    return found;
    }





static char shouldBeEveInjection( float inFitnessScore ) {

    // if we exceed our baby ratio, we're in an emergency situation and
    // need a new eve NOW

    // so this player is the lucky winner, even if their fitness score
    // isn't high enough
    int cMom = countFertileMothers();
    
    int cBaby = countHelplessBabies();

    // this player would be a new baby if not Eve
    cBaby ++;


    float maxBabyRatio = 
        SettingsManager::getFloatSetting( "eveInjectionBabyRatio", 3.0f );

    float babyRatio = cBaby / (float)cMom;

    if( babyRatio > maxBabyRatio ) {
        AppLog::infoF( "Injecting Eve:  "
                       "%d babies, %d moms, ratio %f, max ratio %f",
                       cBaby, cMom, babyRatio, maxBabyRatio );
        return true;
        }

    

    // for other case (not enough families) the situation isn't dire
    // we can wait for someone to come along with a high fitness score
    

    // how many recent players do we look at?
    int eveFitnessWindow =
        SettingsManager::getIntSetting( "eveInjectionFitnessWindow", 10 );

    int numPlayers = players.size();
    float maxFitnessSeen = 0;
    
    for( int i = numPlayers - 1; 
         i >= numPlayers - eveFitnessWindow && i >= 0;
         i -- ) {
        
        LiveObject *o = players.getElement( i );
        if( o->fitnessScore > maxFitnessSeen ) {
            maxFitnessSeen = o->fitnessScore;
            }
        }

    if( inFitnessScore < maxFitnessSeen ) {
        // this player not fit enough to be Eve
        return false;
        }
    
    int lp = countLivingPlayers();
    
    int cFam = countFamilies();

    // this player would add to the player count if not Eve
    lp ++;

    float maxFamRatio = 
        SettingsManager::getFloatSetting( "eveInjectionFamilyRatio", 9.0f );
    
    float famRatio = lp / (float)cFam;
    
    if( famRatio > maxFamRatio ) {
        // not enough fams
        AppLog::infoF( "Injecting Eve:  "
                       "%d players, %d families, ratio %f, max ratio %f",
                       lp, cFam, famRatio, maxFamRatio );
        return true;
        }
    

    return false;
    }






// for placement of tutorials out of the way 
static int maxPlacementX = 5000000;

// tutorial is alwasy placed 400,000 to East of furthest birth/Eve
// location
static int tutorialOffsetX = 400000;


// each subsequent tutorial gets put in a diferent place
static int tutorialCount = 0;



// fill this with emails that should also affect lineage ban
// if any twin in group is banned, all should be
static SimpleVector<char*> tempTwinEmails;

static char nextLogInTwin = false;

// returns ID of new player,
// or -1 if this player reconnected to an existing ID
int processLoggedInPlayer( char inAllowReconnect,
                           Socket *inSock,
                           SimpleVector<char> *inSockBuffer,
                           char *inEmail,
                           int inTutorialNumber,
                           CurseStatus inCurseStatus,
                           PastLifeStats inLifeStats,
                           float inFitnessScore,
                           // set to -2 to force Eve
                           int inForceParentID = -1,
                           int inForceDisplayID = -1,
                           GridPos *inForcePlayerPos = NULL ) {
    

    int usePersonalCurses = SettingsManager::getIntSetting( "usePersonalCurses",
                                                            0 );
    
    if( usePersonalCurses ) {
        // ignore what old curse system said
        inCurseStatus.curseLevel = 0;
        inCurseStatus.excessPoints = 0;
        
        initPersonalCurseTest( inEmail );
        
        for( int p=0; p<players.size(); p++ ) {
            LiveObject *o = players.getElement( p );
        
            if( ! o->error && 
                ! o->isTutorial &&
                o->curseStatus.curseLevel == 0 &&
                strcmp( o->email, inEmail ) != 0 ) {

                // non-tutorial, non-cursed, non-us player
                addPersonToPersonalCurseTest( o->email, inEmail,
                                              getPlayerPos( o ) );
                }
            }
        }
    


    // new behavior:
    // allow this new connection from same
    // email (most likely a re-connect
    // by same person, when the old connection
    // hasn't broken on our end yet)
    
    // to make it work, force-mark
    // the old connection as broken
    for( int p=0; p<players.size(); p++ ) {
        LiveObject *o = players.getElement( p );
        
        if( ! o->error && 
            o->connected && 
            strcmp( o->email, inEmail ) == 0 ) {
            
            setPlayerDisconnected( o, "Authentic reconnect received" );
            
            break;
            }
        }



    // see if player was previously disconnected
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( ! o->error && ! o->connected &&
            strcmp( o->email, inEmail ) == 0 ) {

            if( ! inAllowReconnect ) {
                // trigger an error for them, so they die and are removed
                o->error = true;
                o->errorCauseString = "Reconnected as twin";
                break;
                }
            
            // else allow them to reconnect to existing life

            // give them this new socket and buffer
            if( o->sock != NULL ) {
                delete o->sock;
                o->sock = NULL;
                }
            if( o->sockBuffer != NULL ) {
                delete o->sockBuffer;
                o->sockBuffer = NULL;
                }
            
            o->sock = inSock;
            o->sockBuffer = inSockBuffer;
            
            // they are connecting again, need to send them everything again
            o->firstMapSent = false;
            o->firstMessageSent = false;
            o->inFlight = false;
            
            o->connected = true;
            
            if( o->heldByOther ) {
                // they're held, so they may have moved far away from their
                // original location
                
                // their first PU on reconnect should give an estimate of this
                // new location
                
                LiveObject *holdingPlayer = 
                    getLiveObject( o->heldByOtherID );
                
                if( holdingPlayer != NULL ) {
                    o->xd = holdingPlayer->xd;
                    o->yd = holdingPlayer->yd;
                    
                    o->xs = holdingPlayer->xs;
                    o->ys = holdingPlayer->ys;
                    }
                }
            
            AppLog::infoF( "Player %d (%s) has reconnected.",
                           o->id, o->email );

            delete [] inEmail;
            
            return -1;
            }
        }
    


    // a baby needs to be born

    char eveWindow = isEveWindow();
    char forceGirl = false;
    

    char eveInjectionOn = SettingsManager::getIntSetting( "eveInjectionOn", 0 );
    

    int familyLimitAfterEveWindow = SettingsManager::getIntSetting( 
            "familyLimitAfterEveWindow", 15 );

    int minFamiliesAfterEveWindow = SettingsManager::getIntSetting( 
        "minFamiliesAfterEveWindow", 5 );

    int cM = countFertileMothers();
    int cB = countHelplessBabies();
    int cFam = countFamilies();

    if( ! eveWindow && ! eveInjectionOn ) {
        
        float babyMotherRatio = SettingsManager::getFloatSetting( 
            "babyMotherApocalypseRatio", 6.0 );
        
        float babyPlayerRatio = SettingsManager::getFloatSetting( 
            "babyToPlayerApocalypseRatio", 0.33 );
        
        int cP = countLivingPlayers();

        if( cM == 0 || (float)cB / (float)cM >= babyMotherRatio ) {
            // too many babies per mother inside barrier
            float thisRatio = 0;
            if( cM > 0 ) {
                thisRatio = (float)cB / (float)cM;
                }

            char *logMessage = autoSprintf( 
                "Too many babies per mother inside barrier: "
                "%d mothers, %d babies, %f ratio, %f max ratio",
                cM, cB, thisRatio, babyMotherRatio );
            triggerApocalypseNow( logMessage );
            
            delete [] logMessage;
            }
        else if( cP == 0 || (float)cB / (float)cP >= babyPlayerRatio ) {
            // too many babies per player inside barrier
            float thisRatio = 0;
            if( cP > 0 ) {
                thisRatio = (float)cB / (float)cP;
                }

            char *logMessage = autoSprintf( 
                "Too many babies per player inside barrier: "
                "%d players, %d babies, %f ratio, %f max ratio",
                cP, cB, thisRatio, babyPlayerRatio );
            triggerApocalypseNow( logMessage );
            
            delete [] logMessage;
            }
        else {
            int minFertile = players.size() / 15;
            if( minFertile < 2 ) {
                minFertile = 2;
                }
            if( cM < minFertile ) {
                // less than 1/15 of the players are fertile mothers
                forceGirl = true;
                }
            }

        if( !apocalypseTriggered && familyLimitAfterEveWindow > 0 ) {
            
            // there's a family limit
            // see if we passed it
            
            if( cFam > familyLimitAfterEveWindow ) {
                // too many families
                
                // that means we've reach a state where no one is surviving
                // and there are lots of eves scrounging around
                triggerApocalypseNow( 
                    "Too many families after Eve window closed" );
                }
            }

        if( !apocalypseTriggered ) {
            int maxSeconds =
                SettingsManager::getIntSetting( "arcRunMaxSeconds", 0 );

            if( maxSeconds > 0 &&
                getArcRunningSeconds() > maxSeconds ) {
                // players WON and survived past max seconds
                triggerApocalypseNow( "Arc run exceeded max seconds" );
                }
            }    

        if( !apocalypseTriggered && minFamiliesAfterEveWindow > 0 ) {
            
            if( cFam < minFamiliesAfterEveWindow ) {
                // too many families have died out
                triggerApocalypseNow( "Too few families left" );
                }
            }    

        }

    
    int barrierRadius = SettingsManager::getIntSetting( "barrierRadius", 250 );
    int barrierOn = SettingsManager::getIntSetting( "barrierOn", 1 );
    

    // reload these settings every time someone new connects
    // thus, they can be changed without restarting the server
    minFoodDecrementSeconds = 
        SettingsManager::getFloatSetting( "minFoodDecrementSeconds", 5.0f );
    
    maxFoodDecrementSeconds = 
        SettingsManager::getFloatSetting( "maxFoodDecrementSeconds", 20 );

    foodScaleFactor = 
        SettingsManager::getFloatSetting( "foodScaleFactor", 1.0 );

    babyBirthFoodDecrement = 
        SettingsManager::getIntSetting( "babyBirthFoodDecrement", 10 );

    indoorFoodDecrementSecondsBonus = SettingsManager::getFloatSetting( 
        "indoorFoodDecrementSecondsBonus", 20 );


    eatBonus = 
        SettingsManager::getIntSetting( "eatBonus", 0 );

    minActivePlayersForLanguages =
        SettingsManager::getIntSetting( "minActivePlayersForLanguages", 15 );

    SimpleVector<double> *multiplierList = 
        SettingsManager::getDoubleSettingMulti( "posseSpeedMultipliers" );
    
    for( int i=0; i<multiplierList->size() && i < 4; i++ ) {
        posseSizeSpeedMultipliers[i] = multiplierList->getElementDirect( i );
        }
    delete multiplierList;
    



    numConnections ++;
                
    LiveObject newObject;

    newObject.email = inEmail;
    newObject.origEmail = NULL;
    
    newObject.id = nextID;
    nextID++;


    if( nextLogInTwin ) {
        newObject.isTwin = true;
        }
    else {
        newObject.isTwin = false;
        }
    


    if( familyDataLogFile != NULL ) {
        int eveCount = 0;
        int inCount = 0;
        
        double ageSum = 0;
        int ageSumCount = 0;
        
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *o = players.getElement( i );
        
            if( ! o->error && o->connected ) {
                if( o->parentID == -1 ) {
                    eveCount++;
                    }
                if( barrierOn ) {
                    // only those inside the barrier
                    GridPos pos = getPlayerPos( o );
                
                    if( abs( pos.x ) < barrierRadius &&
                        abs( pos.y ) < barrierRadius ) {
                        inCount++;
                        
                        ageSum += computeAge( o );
                        ageSumCount++;
                        }
                    }
                else {
                    ageSum += computeAge( o );
                    ageSumCount++;
                    }
                }
            }
        
        double averageAge = 0;
        if( ageSumCount > 0 ) {
            averageAge = ageSum / ageSumCount;
            }
        
        fprintf( familyDataLogFile,
                 "%.2f nid:%d fam:%d mom:%d bb:%d plr:%d eve:%d rft:%d "
                 "avAge:%.2f\n",
                 Time::getCurrentTime(), newObject.id, 
                 cFam, cM, cB,
                 players.size(),
                 eveCount,
                 inCount,
                 averageAge );
        }


    
    newObject.fitnessScore = inFitnessScore;
    



    SettingsManager::setSetting( "nextPlayerID",
                                 (int)nextID );


    newObject.responsiblePlayerID = -1;
    
    newObject.killPosseSize = 0;

    newObject.displayID = getRandomPersonObject();
    
    newObject.isEve = false;
    
    newObject.isTutorial = false;
    
    if( inTutorialNumber > 0 ) {
        newObject.isTutorial = true;
        }

    newObject.trueStartTimeSeconds = Time::getCurrentTime();
    newObject.lifeStartTimeSeconds = newObject.trueStartTimeSeconds;
                            

    newObject.lastSayTimeSeconds = Time::getCurrentTime();
    newObject.firstEmoteTimeSeconds = Time::getCurrentTime();
    
    newObject.emoteCountInWindow = 0;
    newObject.emoteCooldown = false;
    

    newObject.heldByOther = false;
    newObject.everHeldByParent = false;
    
    newObject.followingID = -1;
    newObject.leadingColorIndex = -1;

    // everyone should hear about who this player is following
    newObject.followingUpdate = true;
    
    newObject.exileUpdate = false;
    
    
    int numOfAge = 0;

    int numBirthLocationsCurseChecked = 0;
    int numBirthLocationsCurseBlocked = 0;
                            
    int numPlayers = players.size();
                            
    SimpleVector<LiveObject*> parentChoices;
    

    // lower the bad mother limit in low-population situations
    // so that babies aren't stuck with the same low-skill mother over and
    // over
    int badMotherLimit = 2 + numPlayers / 3;

    if( badMotherLimit > 10 ) {
        badMotherLimit = 10;
        }
    
    // with new birth cooldowns, we don't need bad mother limit anymore
    // try making it a non-factor
    badMotherLimit = 9999999;
    
    
    primeLineageTest( numPlayers );
    

    for( int i=0; i<numPlayers; i++ ) {
        LiveObject *player = players.getElement( i );
        
        if( player->error ) {
            continue;
            }

        if( player->isTutorial ) {
            continue;
            }

        if( player->vogMode ) {
            continue;
            }

        if( isFertileAge( player ) ) {
            numOfAge ++;

            
            if( Time::timeSec() < player->birthCoolDown ) {    
                continue;
                }
            
            GridPos motherPos = getPlayerPos( player );

            if( ! isLinePermitted( newObject.email, motherPos ) ) {
                // this line forbidden for new player
                continue;
                }
            
            numBirthLocationsCurseChecked ++;
            
            if( usePersonalCurses &&
                isBirthLocationCurseBlocked( newObject.email, motherPos ) ) {
                // this spot forbidden because someone nearby cursed new player
                numBirthLocationsCurseBlocked++;
                continue;
                }
            
            // test any twins also
            char twinBanned = false;
            for( int s=0; s<tempTwinEmails.size(); s++ ) {
                if( ! isLinePermitted( tempTwinEmails.getElementDirect( s ),
                                       motherPos ) ) {
                    twinBanned = true;
                    break;
                    }
                if( usePersonalCurses &&
                    // non-cached version for twin emails
                    // (otherwise, we interfere with caching done
                    //  for our email)
                    isBirthLocationCurseBlockedNoCache( 
                        tempTwinEmails.getElementDirect( s ), motherPos ) ) {
                    twinBanned = true;
                    
                    numBirthLocationsCurseBlocked++;
                    
                    break;
                    }
                }
            
            if( twinBanned ) {
                continue;
                }
            


            int numPastBabies = player->babyIDs->size();
            
            if( numPastBabies >= badMotherLimit ) {
                int numDead = 0;
                
                for( int b=0; b < numPastBabies; b++ ) {
                    
                    int bID = 
                        player->babyIDs->getElementDirect( b );

                    char bAlive = false;
                    
                    for( int j=0; j<numPlayers; j++ ) {
                        LiveObject *otherObj = players.getElement( j );
                    
                        if( otherObj->error ) {
                            continue;
                            }

                        int id = otherObj->id;
                    
                        if( id == bID ) {
                            bAlive = true;
                            break;
                            }
                        }
                    if( ! bAlive ) {
                        numDead ++;
                        }
                    }
                
                if( numDead >= badMotherLimit ) {
                    // this is a bad mother who lets all babies die
                    // don't give them more babies
                    continue;
                    }
                }


            if( barrierOn ) {
                // only mothers inside barrier can have babies

                GridPos playerPos = getPlayerPos( player );
                
                if( abs( playerPos.x ) >= barrierRadius ||
                    abs( playerPos.y ) >= barrierRadius ) {
                    continue;
                    }
                }


            // got past all other tests

            if( ( inCurseStatus.curseLevel <= 0 && 
                  player->curseStatus.curseLevel <= 0 ) 
                || 
                ( inCurseStatus.curseLevel > 0 && 
                  player->curseStatus.curseLevel > 0 ) ) {
                // cursed babies only born to cursed mothers
                // non-cursed babies never born to cursed mothers
                parentChoices.push_back( player );
                }
            }
        }


    char forceParentChoices = false;
    

    if( inTutorialNumber > 0 ) {
        // Tutorial always played full-grown
        parentChoices.deleteAll();
        forceParentChoices = true;
        }

    if( inForceParentID == -2 ) {
        // force eve
        parentChoices.deleteAll();
        forceParentChoices = true;
        }
    else if( inForceParentID > -1 ) {
        // force parent choice
        parentChoices.deleteAll();
        
        LiveObject *forcedParent = getLiveObject( inForceParentID );
        
        if( forcedParent != NULL ) {
            parentChoices.push_back( forcedParent );
            forceParentChoices = true;
            }
        }
    
    
    char forceSpawn = false;
    ForceSpawnRecord forceSpawnInfo;
    
    if( SettingsManager::getIntSetting( "forceAllPlayersEve", 0 ) ) {
        parentChoices.deleteAll();
        forceParentChoices = true;
        }
    else {
        forceSpawn = getForceSpawn( inEmail, &forceSpawnInfo );
    
        if( forceSpawn ) {
            parentChoices.deleteAll();
            forceParentChoices = true;
            }
        }
    



    if( ! eveWindow && eveInjectionOn &&
        ! forceParentChoices &&
        // this player not curse blocked by all possible mothers
        ( numBirthLocationsCurseBlocked == 0 ||
          numBirthLocationsCurseBlocked < numBirthLocationsCurseChecked ) ) {
        
        // should we spawn a new "special" eve outside of Eve window?
        if( shouldBeEveInjection( newObject.fitnessScore ) ) {
            parentChoices.deleteAll();
            forceParentChoices = true;
            }
        }
    


    
    if( ( eveWindow || familyLimitAfterEveWindow > 0 ) 
        && parentChoices.size() > 0 ) {
        // count the families, and add new Eve if there are too
        // few families for the playerbase 
        // (but only if in pure Eve window )
        // (    OR tracking family limit after window closes)

        SimpleVector<int> uniqueLines;
        
        int playerCount = 0;
        
        for( int i=0; i<numPlayers; i++ ) {
            LiveObject *player = players.getElement( i );
            
            if( player->error ) {
                continue;
                }
            playerCount++;

            int lineageEveID = player->lineageEveID;
            
            if( uniqueLines.getElementIndex( lineageEveID ) == -1 ) {
                uniqueLines.push_back( lineageEveID );
                }
            }
        
        int numLines = uniqueLines.size();
        
        int targetPerFamily = 
            SettingsManager::getIntSetting( "targetPlayersPerFamily", 15 );
        
        int actual = playerCount / numLines;
        
        AppLog::infoF( "%d players on server in %d family lines, with "
                       "%d players per family, average.  Target is %d "
                       "per family.",
                       playerCount, numLines, actual, targetPerFamily );

        if( actual > targetPerFamily ) {
            
            AppLog::info( "Over target, adding a new Eve." );
            
            parentChoices.deleteAll();
            forceParentChoices = true;
            }
        
        }
    



    if( inCurseStatus.curseLevel <= 0 &&
        ! forceParentChoices && 
        parentChoices.size() == 0 &&
        ! ( eveWindow || familyLimitAfterEveWindow > 0 ) &&
        ! apocalypseTriggered ) {
        
        // outside pure Eve window (and not tracking family limit after)
        //
        // and no mother choices left (based on lineage 
        // bans or birth cooldowns)
        
        char anyCurseBlocked = false;

        // consider all fertile mothers
        for( int i=0; i<numPlayers; i++ ) {
            LiveObject *player = players.getElement( i );
        
            if( player->error ) {
                continue;
                }
            
            if( player->isTutorial ) {
                continue;
                }
            
            if( player->vogMode ) {
                continue;
                }
            
            if( player->curseStatus.curseLevel > 0 ) {
                continue;
                }
            
            GridPos playerPos = getPlayerPos( player );
            
            if( usePersonalCurses && 
                isBirthLocationCurseBlocked( newObject.email, 
                                             playerPos ) ) {
                // this spot forbidden because someone nearby cursed new player
                anyCurseBlocked = true;
                continue;
                }
            
            char twinBanned = false;
            if( usePersonalCurses ) {
                for( int s=0; s<tempTwinEmails.size(); s++ ) {
                    if( isBirthLocationCurseBlockedNoCache( 
                            tempTwinEmails.getElementDirect( s ), 
                            playerPos ) ) {
                        twinBanned = true;
                        break;
                        }
                    }
                }
            
            if( twinBanned ) {
                anyCurseBlocked = true;
                continue;
                }


            if( barrierOn ) {
                // only mothers inside barrier can have babies
                
                if( abs( playerPos.x ) >= barrierRadius ||
                    abs( playerPos.y ) >= barrierRadius ) {
                    continue;
                    }
                }
            
            
            if( isFertileAge( player ) ) {
                parentChoices.push_back( player );
                }
            }

        if( parentChoices.size() == 0 ) {
            if( anyCurseBlocked ) {
                // only fertile mothers are blocked for this cursed player
                // send this player to donkeytown
                inCurseStatus.curseLevel = 1;
                inCurseStatus.excessPoints = 1;
                }
            else {
                
                // absolutely no fertile mothers on server
                
                // the in-barrier mothers we found before must have aged out
                // along the way
                
                triggerApocalypseNow( "No fertile mothers left on server" );
                }
            }
        }
    

    if( inCurseStatus.curseLevel <= 0 &&
        ! forceParentChoices && 
        parentChoices.size() == 0 &&
        eveWindow &&
        ! apocalypseTriggered &&
        usePersonalCurses ) {
        // still in Eve window, and found no choices for this player
        // to be born to.

        // let's check if ALL fertile mothers have them curse-blocked,
        // currently.  If so, and that's the reason we could find
        // no mother for them, send them to d-town
        
        char someFertileNotCurseBlocked = false;
        char someFertileCurseBlocked = false;
        
        // consider all fertile mothers
        for( int i=0; i<numPlayers; i++ ) {
            LiveObject *player = players.getElement( i );
        
            if( player->error ) {
                continue;
                }
            
            if( player->isTutorial ) {
                continue;
                }
            
            if( player->vogMode ) {
                continue;
                }
            
            if( player->curseStatus.curseLevel > 0 ) {
                continue;
                }

            if( isFertileAge( player ) ) {
                GridPos playerPos = getPlayerPos( player );
                
                if( isBirthLocationCurseBlocked( newObject.email,
                                                 playerPos ) ) {
                    // this spot forbidden because 
                    // someone nearby cursed new player
                    someFertileCurseBlocked = true;
                    continue;
                    }

                char twinBanned = false;
                if( usePersonalCurses ) {
                    for( int s=0; s<tempTwinEmails.size(); s++ ) {
                        if( isBirthLocationCurseBlockedNoCache( 
                                tempTwinEmails.getElementDirect( s ), 
                                playerPos ) ) {
                            twinBanned = true;
                            break;
                            }
                        }
                    }
                
                if( twinBanned ) {
                    someFertileCurseBlocked = true;
                    continue;
                    }

                
                someFertileNotCurseBlocked = true;
                break;
                }
            }

        if( someFertileCurseBlocked && ! someFertileNotCurseBlocked ) {
            // they are blocked from being born EVERYWHERE by curses

            // d-town
            inCurseStatus.curseLevel = 1;
            inCurseStatus.excessPoints = 1;
            }
        }
    



    newObject.parentChainLength = 1;

    if( parentChoices.size() == 0 || numOfAge == 0 ) {
        // new Eve
        // she starts almost full grown

        newObject.isEve = true;
        newObject.lineageEveID = newObject.id;
        
        newObject.lifeStartTimeSeconds -= 14 * ( 1.0 / getAgeRate() );

        
        // when placing eve, pick a race that is not currently
        // represented
        int numRaces = 0;
        int *races = getRaces( &numRaces );
        
        int *counts = new int[ numRaces ];
        
        int foundMin = -1;
        int minFem = 999999;
        
        // first, shuffle races
        for( int r=0; r<numRaces; r++ ) {
            int other = randSource.getRandomBoundedInt( 0, numRaces - 1 );
            int temp = races[r];
            races[r] = races[other];
            races[other] = temp;
            }

        for( int r=0; r<numRaces; r++ ) {
            counts[r] = 0;
            for( int i=0; i<numPlayers; i++ ) {
                LiveObject *player = players.getElement( i );
            
                if( isPlayerCountable( player ) && isFertileAge( player ) ) {
                    ObjectRecord *d = getObject( player->displayID );
                    
                    if( d->race == races[r] ) {
                        counts[r] ++;
                        }
                    }
                }
            if( counts[r] == 0 &&
                getRaceSize( races[r] ) >= 2 ) {
                foundMin = races[r];
                break;
                }
            else if( counts[r] > 0 && counts[r] < minFem ) {
                minFem = counts[r];
                foundMin = races[r];
                }
            }
        
        delete [] races;
        delete [] counts;
        

        int femaleID = -1;
        
        if( foundMin != -1 ) {
            femaleID = getRandomPersonObjectOfRace( foundMin );
            
            int tryCount = 0;
            while( getObject( femaleID )->male && tryCount < 10 ) {
                femaleID = getRandomPersonObjectOfRace( foundMin );
                tryCount++;
                }
            if( getObject( femaleID )->male ) {
                femaleID = -1;
                }
            }

        if( femaleID == -1 ) {       
            // all races present, or couldn't find female character
            // to use in weakest race
            femaleID = getRandomFemalePersonObject();
            }
        
        if( femaleID != -1 ) {
            newObject.displayID = femaleID;
            }
        }
    

    if( !forceParentChoices && 
        parentChoices.size() == 0 && numOfAge == 0 && 
        inCurseStatus.curseLevel == 0 ) {
        // all existing babies are good spawn spot for Eve
                    
        for( int i=0; i<numPlayers; i++ ) {
            LiveObject *player = players.getElement( i );
            
            if( player->error ) {
                continue;
                }

            if( computeAge( player ) < babyAge ) {
                parentChoices.push_back( player );
                }
            }
        }
    else {
        // testing
        //newObject.lifeStartTimeSeconds -= 14 * ( 1.0 / getAgeRate() );
        }
    
                
    // else player starts as newborn
                

    newObject.foodCapModifier = 1.0;

    newObject.fever = 0;

    // start full up to capacity with food
    newObject.foodStore = computeFoodCapacity( &newObject );

    newObject.drunkenness = 0;
    

    if( ! newObject.isEve ) {
        // babies start out almost starving
        newObject.foodStore = 2;
        }
    
    if( newObject.isTutorial && newObject.foodStore > 10 ) {
        // so they can practice eating at the beginning of the tutorial
        newObject.foodStore -= 6;
        }
    

    newObject.envHeat = targetHeat;
    newObject.bodyHeat = targetHeat;
    newObject.biomeHeat = targetHeat;
    newObject.lastBiomeHeat = targetHeat;
    newObject.heat = 0.5;
    newObject.heatUpdate = false;
    newObject.lastHeatUpdate = Time::getCurrentTime();
    newObject.isIndoors = false;
    
    newObject.foodDrainTime = 0;
    newObject.indoorBonusTime = 0;
    newObject.indoorBonusFraction = 0;


    newObject.foodDecrementETASeconds =
        Time::getCurrentTime() + 
        computeFoodDecrementTimeSeconds( &newObject );
                
    newObject.foodUpdate = true;
    newObject.lastAteID = 0;
    newObject.lastAteFillMax = 0;
    newObject.justAte = false;
    newObject.justAteID = 0;
    
    newObject.yummyBonusStore = 0;

    newObject.lastReportedFoodCapacity = 0;

    newObject.clothing = getEmptyClothingSet();

    for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
        newObject.clothingEtaDecay[c] = 0;
        }
    
    newObject.xs = 0;
    newObject.ys = 0;
    newObject.xd = 0;
    newObject.yd = 0;
    
    newObject.mapChunkPathCheckedDest.x = 0;
    newObject.mapChunkPathCheckedDest.y = 0;
    

    newObject.lastRegionLookTime = 0;
    newObject.playerCrossingCheckTime = 0;
    
    
    LiveObject *parent = NULL;

    char placed = false;
    
    if( parentChoices.size() > 0 ) {
        placed = true;
        
        if( newObject.isEve ) {
            // spawned next to random existing player
            int parentIndex = 
                randSource.getRandomBoundedInt( 0,
                                                parentChoices.size() - 1 );
            
            parent = parentChoices.getElementDirect( parentIndex );
            }
        else {
            // baby


            
            // filter parent choices by this baby's skip list
            SimpleVector<LiveObject *> 
                filteredParentChoices( parentChoices.size() );
            
            for( int i=0; i<parentChoices.size(); i++ ) {
                LiveObject *p = parentChoices.getElementDirect( i );
                
                if( ! isSkipped( inEmail, p->lineageEveID ) ) {
                    filteredParentChoices.push_back( p );
                    }
                }

            if( filteredParentChoices.size() == 0 ) {
                // baby has skipped everyone
                
                // clear their list and let them start over again
                clearSkipList( inEmail );
                
                filteredParentChoices.push_back_other( &parentChoices );
                }
            

            
            // pick random mother from a weighted distribution based on 
            // each mother's temperature
            
            // AND each mother's current YUM multiplier
            
            int maxYumMult = 1;

            for( int i=0; i<filteredParentChoices.size(); i++ ) {
                LiveObject *p = filteredParentChoices.getElementDirect( i );
                
                int yumMult = p->yummyFoodChain.size() - 1;
                
                if( yumMult < 0 ) {
                    yumMult = 0;
                    }
                
                if( yumMult > maxYumMult ) {
                    maxYumMult = yumMult;
                    }
                }
            
            // 0.5 temp is worth .5 weight
            // 1.0 temp and 0 are worth 0 weight
            
            // max YumMult worth same that perfect temp is worth (0.5 weight)


            // after Eve window, give each baby to a different family
            // round-robin
            int pickedFamLineageEveID = -1;
            
            if( ! eveWindow && familyLimitAfterEveWindow == 0 ) {    
                pickedFamLineageEveID = getNextBabyFamilyLineageEveID();
                }
            

            double totalWeight = 0;
            
            SimpleVector<double> filteredParentChoiceWeights;
            
            for( int i=0; i<filteredParentChoices.size(); i++ ) {
                LiveObject *p = filteredParentChoices.getElementDirect( i );

                // temp part of weight
                double thisMotherWeight = 0.5 - fabs( p->heat - 0.5 );
                

                int yumMult = p->yummyFoodChain.size() - 1;
                                
                if( yumMult < 0 ) {
                    yumMult = 0;
                    }

                // yum mult part of weight
                thisMotherWeight += 0.5 * yumMult / (double) maxYumMult;
                
                if( pickedFamLineageEveID != -1 &&
                    p->lineageEveID == pickedFamLineageEveID ) {
                    // this is chosen family
                    // multiply their weights by 1000x to make
                    // them inevitable
                    // (they will still compete with each other)
                    thisMotherWeight *= 1000;
                    }
                
                filteredParentChoiceWeights.push_back( thisMotherWeight );
                
                totalWeight += thisMotherWeight;
                }

            double choice = 
                randSource.getRandomBoundedDouble( 0, totalWeight );
            
            
            totalWeight = 0;
            
            for( int i=0; i<filteredParentChoices.size(); i++ ) {
                LiveObject *p = filteredParentChoices.getElementDirect( i );
                
                totalWeight += 
                    filteredParentChoiceWeights.getElementDirect( i );

                if( totalWeight >= choice ) {
                    parent = p;
                    break;
                    }                
                }

            if( parent != NULL ) {
                // check if this family has too few potentially fertile
                // females
                // If so, force a girl baby.
                // Do this regardless of whether Eve window is in effect, etc.
                int min = SettingsManager::getIntSetting( 
                    "minPotentialFertileFemalesPerFamily", 3 );
                int famMothers = countFertileMothers( parent->lineageEveID );
                int famGirls = countGirls( parent->lineageEveID );
                if( famMothers + famGirls < min ) {
                    forceGirl = true;
                    }
                }
            }
        

        
        if( ! newObject.isEve ) {
            // mother giving birth to baby
            // take a ton out of her food store

            int min = 4;
            if( parent->foodStore < min ) {
                min = parent->foodStore;
                }
            parent->foodStore -= babyBirthFoodDecrement;
            if( parent->foodStore < min ) {
                parent->foodStore = min;
                }

            parent->foodDecrementETASeconds +=
                computeFoodDecrementTimeSeconds( parent );
            
            parent->foodUpdate = true;
            

            // only set race if the spawn-near player is our mother
            // otherwise, we are a new Eve spawning next to a baby
            
            timeSec_t curTime = Time::timeSec();
            
            parent->babyBirthTimes->push_back( curTime );
            parent->babyIDs->push_back( newObject.id );
            
            // set cool-down time before this worman can have another baby
            parent->birthCoolDown = pickBirthCooldownSeconds() + curTime;

            ObjectRecord *parentObject = getObject( parent->displayID );

            // pick race of child
            int numRaces;
            int *races = getRaces( &numRaces );
        
            int parentRaceIndex = -1;
            
            for( int i=0; i<numRaces; i++ ) {
                if( parentObject->race == races[i] ) {
                    parentRaceIndex = i;
                    break;
                    }
                }
            

            if( parentRaceIndex != -1 ) {
                
                int childRace = parentObject->race;
                
                char forceDifferentRace = false;

                if( getRaceSize( parentObject->race ) < 3 ) {
                    // no room in race for diverse family members
                    
                    // pick a different race for child to ensure village 
                    // diversity
                    // (otherwise, almost everyone is going to look the same)
                    forceDifferentRace = true;
                    }
                
                // everyone has a small chance of having a neighboring-race
                // baby, even if not forced by parent's small race size
                if( forceDifferentRace ||
                    randSource.getRandomDouble() > 
                    childSameRaceLikelihood ) {
                    
                    // different race than parent
                    
                    int offset = 1;
                    
                    if( randSource.getRandomBoolean() ) {
                        offset = -1;
                        }
                    int childRaceIndex = parentRaceIndex + offset;
                    
                    // don't wrap around
                    // but push in other direction instead
                    if( childRaceIndex >= numRaces ) {
                        childRaceIndex = numRaces - 2;
                        }
                    if( childRaceIndex < 0 ) {
                        childRaceIndex = 1;
                        }
                    
                    // stay in bounds
                    if( childRaceIndex >= numRaces ) {
                        childRaceIndex = numRaces - 1;
                        }
                    

                    childRace = races[ childRaceIndex ];
                    }
                
                if( childRace == parentObject->race ) {
                    newObject.displayID = getRandomFamilyMember( 
                        parentObject->race, parent->displayID, familySpan,
                        forceGirl );
                    }
                else {
                    newObject.displayID = 
                        getRandomPersonObjectOfRace( childRace );
                    }
            
                }
        
            delete [] races;
            }
        
        if( parent->xs == parent->xd && 
            parent->ys == parent->yd ) {
                        
            // stationary parent
            newObject.xs = parent->xs;
            newObject.ys = parent->ys;
                        
            newObject.xd = parent->xs;
            newObject.yd = parent->ys;
            }
        else {
            // find where parent is along path
            GridPos cPos = computePartialMoveSpot( parent );
                        
            newObject.xs = cPos.x;
            newObject.ys = cPos.y;
                        
            newObject.xd = cPos.x;
            newObject.yd = cPos.y;
            }
        
        if( newObject.xs > maxPlacementX ) {
            maxPlacementX = newObject.xs;
            }
        }
    else if( inTutorialNumber > 0 ) {
        
        int startX = maxPlacementX + tutorialOffsetX;
        int startY = tutorialCount * 40;

        newObject.xs = startX;
        newObject.ys = startY;
        
        newObject.xd = startX;
        newObject.yd = startY;

        char *mapFileName = autoSprintf( "tutorial%d.txt", inTutorialNumber );
        
        placed = loadTutorialStart( &( newObject.tutorialLoad ),
                                    mapFileName, startX, startY );
        
        delete [] mapFileName;

        tutorialCount ++;

        int maxPlayers = 
            SettingsManager::getIntSetting( "maxPlayers", 200 );

        if( tutorialCount > maxPlayers ) {
            // wrap back to 0 so we don't keep getting farther
            // and farther away on map if server runs for a long time.

            // The earlier-placed tutorials are over by now, because
            // we can't have more than maxPlayers tutorials running at once
            
            tutorialCount = 0;
            }
        }
    
    if( inForcePlayerPos != NULL ) {
        placed = true;

        int startX = inForcePlayerPos->x;
        int startY = inForcePlayerPos->y;
        
        newObject.xs = startX;
        newObject.ys = startY;
        
        newObject.xd = startX;
        newObject.yd = startY;
        }
    
    
    if( !placed ) {
        // tutorial didn't happen if not placed
        newObject.isTutorial = false;
        
        char allowEveRespawn = true;
        
        if( numOfAge >= 4 ) {
            // there are at least 4 fertile females on the server
            // why is this player spawning as Eve?
            // they must be on lineage ban everywhere OR a forced Eve injection
            // (and they are NOT a solo player on an empty server)
            // don't allow them to spawn back at their last old-age Eve death
            // location.
            allowEveRespawn = false;
            }

        // else starts at civ outskirts (lone Eve)
        
        SimpleVector<GridPos> otherPeoplePos( numPlayers );


        // consider players to be near Eve location that match
        // Eve's curse status
        char seekingCursed = false;
        
        if( inCurseStatus.curseLevel > 0 ) {
            seekingCursed = true;
            }
        

        for( int i=0; i<numPlayers; i++ ) {
            LiveObject *player = players.getElement( i );
            
            if( player->error || 
                ! player->connected ||
                player->isTutorial ||
                player->vogMode ) {
                continue;
                }

            if( seekingCursed && player->curseStatus.curseLevel <= 0 ) {
                continue;
                }
            else if( ! seekingCursed &&
                     player->curseStatus.curseLevel > 0 ) {
                continue;
                }

            GridPos p = { player->xs, player->ys };
            otherPeoplePos.push_back( p );
            }
        

        char incrementEvePlacement = true;        

        // don't increment Eve placement if this is a cursed player
        if( inCurseStatus.curseLevel > 0 ) {
            incrementEvePlacement = false;
            }

        int startX, startY;
        getEvePosition( newObject.email, 
                        newObject.id, &startX, &startY, 
                        &otherPeoplePos, allowEveRespawn, 
                        incrementEvePlacement );

        if( inCurseStatus.curseLevel > 0 ) {
            // keep cursed players away

            // 20K away in X and 20K away in Y, pushing out away from 0
            // in both directions

            if( startX > 0 )
                startX += 20000;
            else
                startX -= 20000;
            
            if( startY > 0 )
                startY += 20000;
            else
                startY -= 20000;
            }
        

        if( SettingsManager::getIntSetting( "forceEveLocation", 0 ) ) {

            startX = 
                SettingsManager::getIntSetting( "forceEveLocationX", 0 );
            startY = 
                SettingsManager::getIntSetting( "forceEveLocationY", 0 );
            }
        
        
        newObject.xs = startX;
        newObject.ys = startY;
        
        newObject.xd = startX;
        newObject.yd = startY;

        if( newObject.xs > maxPlacementX ) {
            maxPlacementX = newObject.xs;
            }
        }
    

    if( inForceDisplayID != -1 ) {
        newObject.displayID = inForceDisplayID;
        }


    
    if( parent == NULL ) {
        // Eve
        int forceID = SettingsManager::getIntSetting( "forceEveObject", 0 );
    
        if( forceID > 0 ) {
            newObject.displayID = forceID;
            }
        
        
        float forceAge = SettingsManager::getFloatSetting( "forceEveAge", 0.0 );
        
        if( forceAge > 0 ) {
            newObject.lifeStartTimeSeconds = 
                Time::getCurrentTime() - forceAge * ( 1.0 / getAgeRate() );
            }
        }
    

    newObject.holdingID = 0;


    if( areTriggersEnabled() ) {
        int id = getTriggerPlayerDisplayID( inEmail );
        
        if( id != -1 ) {
            newObject.displayID = id;
            
            newObject.lifeStartTimeSeconds = 
                Time::getCurrentTime() - 
                getTriggerPlayerAge( inEmail ) * ( 1.0 / getAgeRate() );
        
            GridPos pos = getTriggerPlayerPos( inEmail );
            
            newObject.xd = pos.x;
            newObject.yd = pos.y;
            newObject.xs = pos.x;
            newObject.ys = pos.y;
            newObject.xd = pos.x;
            
            newObject.holdingID = getTriggerPlayerHolding( inEmail );
            newObject.clothing = getTriggerPlayerClothing( inEmail );
            }
        }
    
    
    newObject.lineage = new SimpleVector<int>();
    
    newObject.name = NULL;
    newObject.familyName = NULL;
    
    newObject.nameHasSuffix = false;
    newObject.lastSay = NULL;
    newObject.curseStatus = inCurseStatus;
    newObject.lifeStats = inLifeStats;
    

    if( newObject.curseStatus.curseLevel == 0 &&
        ! newObject.isTwin &&
        hasCurseToken( inEmail ) ) {
        newObject.curseTokenCount = 1;
        }
    else {
        newObject.curseTokenCount = 0;
        }

    newObject.curseTokenUpdate = true;

    
    newObject.pathLength = 0;
    newObject.pathToDest = NULL;
    newObject.pathTruncated = 0;
    newObject.firstMapSent = false;
    newObject.lastSentMapX = 0;
    newObject.lastSentMapY = 0;
    newObject.moveStartTime = Time::getCurrentTime();
    newObject.moveTotalSeconds = 0;
    newObject.facingOverride = 0;
    newObject.actionAttempt = 0;
    newObject.actionTarget.x = 0;
    newObject.actionTarget.y = 0;
    newObject.holdingEtaDecay = 0;
    newObject.heldOriginValid = 0;
    newObject.heldOriginX = 0;
    newObject.heldOriginY = 0;

    newObject.heldGraveOriginX = 0;
    newObject.heldGraveOriginY = 0;
    newObject.heldGravePlayerID = 0;
    
    newObject.heldTransitionSourceID = -1;
    newObject.numContained = 0;
    newObject.containedIDs = NULL;
    newObject.containedEtaDecays = NULL;
    newObject.subContainedIDs = NULL;
    newObject.subContainedEtaDecays = NULL;
    newObject.embeddedWeaponID = 0;
    newObject.embeddedWeaponEtaDecay = 0;
    newObject.murderSourceID = 0;
    newObject.holdingWound = false;
    newObject.holdingBiomeSickness = false;
    
    newObject.murderPerpID = 0;
    newObject.murderPerpEmail = NULL;
    
    newObject.deathSourceID = 0;
    
    newObject.everKilledAnyone = false;
    newObject.suicide = false;
    

    newObject.sock = inSock;
    newObject.sockBuffer = inSockBuffer;
    
    newObject.gotPartOfThisFrame = false;
    
    newObject.isNew = true;
    newObject.firstMessageSent = false;
    newObject.inFlight = false;
    
    newObject.dying = false;
    newObject.dyingETA = 0;
    
    newObject.emotFrozen = false;
    newObject.emotUnfreezeETA = 0;
    newObject.emotFrozenIndex = 0;
    

    newObject.connected = true;
    newObject.error = false;
    newObject.errorCauseString = "";
    
    newObject.customGraveID = -1;
    newObject.deathReason = NULL;
    
    newObject.deleteSent = false;
    newObject.deathLogged = false;
    newObject.newMove = false;
    
    newObject.posForced = false;
    newObject.waitingForForceResponse = false;
    
    // first move that player sends will be 2
    newObject.lastMoveSequenceNumber = 1;

    newObject.needsUpdate = false;
    newObject.updateSent = false;
    newObject.updateGlobal = false;
    
    newObject.wiggleUpdate = false;

    newObject.babyBirthTimes = new SimpleVector<timeSec_t>();
    newObject.babyIDs = new SimpleVector<int>();
    
    newObject.birthCoolDown = 0;
    
    newObject.monumentPosSet = false;
    newObject.monumentPosSent = true;
    
    newObject.holdingFlightObject = false;

    newObject.vogMode = false;
    newObject.postVogMode = false;
    newObject.vogJumpIndex = 0;
    
    newObject.forceSpawn = false;

    newObject.forceFlightDestSetTime = 0;
                
    for( int i=0; i<HEAT_MAP_D * HEAT_MAP_D; i++ ) {
        newObject.heatMap[i] = 0;
        }

    
    newObject.parentID = -1;
    char *parentEmail = NULL;

    if( parent != NULL && isFertileAge( parent ) ) {
        // do not log babies that new Eve spawns next to as parents
        newObject.parentID = parent->id;
        parentEmail = parent->email;

        if( parent->familyName != NULL ) {
            newObject.familyName = stringDuplicate( parent->familyName );
            }

        newObject.lineageEveID = parent->lineageEveID;

        // child inherits mother's leader
        newObject.followingID = parent->followingID;
        

        newObject.parentChainLength = parent->parentChainLength + 1;

        // mother
        newObject.lineage->push_back( newObject.parentID );

        // inherit last heard monument, if any, from parent
        newObject.monumentPosSet = parent->monumentPosSet;
        newObject.lastMonumentPos = parent->lastMonumentPos;
        newObject.lastMonumentID = parent->lastMonumentID;
        if( newObject.monumentPosSet ) {
            newObject.monumentPosSent = false;
            }
        
        
        for( int i=0; 
             i < parent->lineage->size() && 
                 i < maxLineageTracked - 1;
             i++ ) {
            
            newObject.lineage->push_back( 
                parent->lineage->getElementDirect( i ) );
            }

        if( strstr( newObject.email, "paxkiosk" ) ) {
            // whoa, this baby is a PAX player!
            // let the mother know
            sendGlobalMessage( 
                (char*)"YOUR BABY IS A NEW PLAYER FROM THE PAX EXPO BOOTH.**"
                "PLEASE HELP THEM LEARN THE GAME.  THANKS!  -JASON",
                parent );
            }
        else if( isUsingStatsServer() && 
                 ! newObject.lifeStats.error &&
                 ( newObject.lifeStats.lifeCount < 
                   SettingsManager::getIntSetting( "newPlayerLifeCount", 5 ) ||
                   newObject.lifeStats.lifeTotalSeconds < 
                   SettingsManager::getIntSetting( "newPlayerLifeTotalSeconds",
                                                   7200 ) ) ) {
            // a new player (not at a PAX kiosk)
            // let mother know
            char *motherMessage =  
                SettingsManager::getSettingContents( 
                    "newPlayerMessageForMother", "" );
            
            if( strcmp( motherMessage, "" ) != 0 ) {
                sendGlobalMessage( motherMessage, parent );
                }
            
            delete [] motherMessage;
            }
        }

    if( forceSpawn ) {
        newObject.forceSpawn = true;
        newObject.xs = forceSpawnInfo.pos.x;
        newObject.ys = forceSpawnInfo.pos.y;
        newObject.xd = forceSpawnInfo.pos.x;
        newObject.yd = forceSpawnInfo.pos.y;
        
        newObject.birthPos = forceSpawnInfo.pos;
        
        newObject.lifeStartTimeSeconds = 
            Time::getCurrentTime() -
            forceSpawnInfo.age * ( 1.0 / getAgeRate() );
        
        newObject.name = autoSprintf( "%s %s", 
                                      forceSpawnInfo.firstName,
                                      forceSpawnInfo.lastName );
        newObject.displayID = forceSpawnInfo.displayID;
        
        newObject.clothing.hat = getObject( forceSpawnInfo.hatID, true );
        newObject.clothing.tunic = getObject( forceSpawnInfo.tunicID, true );
        newObject.clothing.bottom = getObject( forceSpawnInfo.bottomID, true );
        newObject.clothing.frontShoe = 
            getObject( forceSpawnInfo.frontShoeID, true );
        newObject.clothing.backShoe = 
            getObject( forceSpawnInfo.backShoeID, true );

        delete [] forceSpawnInfo.firstName;
        delete [] forceSpawnInfo.lastName;
        }
    

    newObject.birthPos.x = newObject.xd;
    newObject.birthPos.y = newObject.yd;
    
    newObject.originalBirthPos = newObject.birthPos;
    

    newObject.heldOriginX = newObject.xd;
    newObject.heldOriginY = newObject.yd;
    
    newObject.actionTarget = newObject.birthPos;



    newObject.ancestorIDs = new SimpleVector<int>();
    newObject.ancestorEmails = new SimpleVector<char*>();
    newObject.ancestorRelNames = new SimpleVector<char*>();
    newObject.ancestorLifeStartTimeSeconds = new SimpleVector<double>();
    
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *otherPlayer = players.getElement( j );
        
        if( otherPlayer->error ) {
            continue;
            }
        
        // a living other player
        
        if( ! getFemale( otherPlayer ) ) {
            
            // check if his mother is an ancestor
            // (then he's an uncle
            if( otherPlayer->parentID > 0 ) {
                
                // look at lineage above parent
                // don't count brothers, only uncles
                for( int i=1; i<newObject.lineage->size(); i++ ) {
                    
                    if( newObject.lineage->getElementDirect( i ) ==
                        otherPlayer->parentID ) {
                        
                        newObject.ancestorIDs->push_back( otherPlayer->id );

                        newObject.ancestorEmails->push_back( 
                            stringDuplicate( otherPlayer->email ) );

                        // i tells us how many greats
                        SimpleVector<char> workingName;
                        
                        for( int g=2; g<=i; g++ ) {
                            workingName.appendElementString( "Great_" );
                            }
                        if( ! getFemale( &newObject ) ) {
                            workingName.appendElementString( "Nephew" );
                            }
                        else {
                            workingName.appendElementString( "Niece" );
                            }

                        newObject.ancestorRelNames->push_back(
                            workingName.getElementString() );
                        
                        newObject.ancestorLifeStartTimeSeconds->push_back(
                            otherPlayer->lifeStartTimeSeconds );
                        
                        break;
                        }
                    }
                }
            }
        else {
            // females, look for direct ancestry

            for( int i=0; i<newObject.lineage->size(); i++ ) {
                    
                if( newObject.lineage->getElementDirect( i ) ==
                    otherPlayer->id ) {
                        
                    newObject.ancestorIDs->push_back( otherPlayer->id );

                    newObject.ancestorEmails->push_back( 
                        stringDuplicate( otherPlayer->email ) );

                    // i tells us how many greats and grands
                    SimpleVector<char> workingName;
                    SimpleVector<char> workingMotherName;
                    
                    for( int g=1; g<=i; g++ ) {
                        if( g == i ) {
                            workingName.appendElementString( "Grand" );
                            workingMotherName.appendElementString( "Grand" );
                            }
                        else {
                            workingName.appendElementString( "Great_" );
                            workingMotherName.appendElementString( "Great_" );
                            }
                        }
                    
                    
                    if( i != 0 ) {
                        if( ! getFemale( &newObject ) ) {
                            workingName.appendElementString( "son" );
                            }
                        else {
                            workingName.appendElementString( "daughter" );
                            }
                        workingMotherName.appendElementString( "mother" );
                        }
                    else {
                        // no "Grand"
                        if( ! getFemale( &newObject ) ) {
                                workingName.appendElementString( "Son" );
                            }
                        else {
                            workingName.appendElementString( "Daughter" );
                            }
                        workingMotherName.appendElementString( "Mother" );
                        }
                    
                    
                    newObject.ancestorRelNames->push_back(
                        workingName.getElementString() );
                    
                    newObject.ancestorLifeStartTimeSeconds->push_back(
                            otherPlayer->lifeStartTimeSeconds );
                    
                    // this is the only case of bi-directionality
                    // players should try to prevent their mothers, gma,
                    // ggma, etc from dying

                    otherPlayer->ancestorIDs->push_back( newObject.id );
                    otherPlayer->ancestorEmails->push_back( 
                        stringDuplicate( newObject.email ) );
                    otherPlayer->ancestorRelNames->push_back( 
                        workingMotherName.getElementString() );
                    otherPlayer->ancestorLifeStartTimeSeconds->push_back(
                        newObject.lifeStartTimeSeconds );
                    
                    break;
                    }
                }
            }
        
        // if we got here, they aren't our mother, g-ma, g-g-ma, etc.
        // nor are they our uncle

        
        // are they our sibling?
        // note that this is only uni-directional
        // (we're checking here for this new baby born)
        // so only our OLDER sibs count as our ancestors (and thus
        // they care about protecting us).

        // this is a little weird, but it does make some sense
        // you are more protective of little sibs

        // anyway, the point of this is to close the "just care about yourself
        // and avoid having kids" exploit.  If your mother has kids after you
        // (which is totally out of your control), then their survival
        // will affect your score.
        
        if( newObject.parentID > 0 &&
            newObject.parentID == otherPlayer->parentID ) {
            // sibs
            
            newObject.ancestorIDs->push_back( otherPlayer->id );

            newObject.ancestorEmails->push_back( 
                stringDuplicate( otherPlayer->email ) );

            const char *relName;
            
            if( ! getFemale( &newObject ) ) {
                relName = "Little_Brother";
                }
            else {
                relName = "Little_Sister";
                }

            newObject.ancestorRelNames->push_back( stringDuplicate( relName ) );
            
            newObject.ancestorLifeStartTimeSeconds->push_back(
                otherPlayer->lifeStartTimeSeconds );
                        
            break;
            }
        }
    

    

    
    // parent pointer possibly no longer valid after push_back, which
    // can resize the vector
    parent = NULL;

    newObject.numToolSlots = -1;
    

    if( newObject.isTutorial ) {
        AppLog::infoF( "New player %s pending tutorial load (tutorial=%d)",
                       newObject.email,
                       inTutorialNumber );

        // holding bay for loading tutorial maps incrementally
        tutorialLoadingPlayers.push_back( newObject );
        }
    else {
        players.push_back( newObject );            
        }

    if( newObject.isEve ) {
        addEveLanguage( newObject.id );
        }
    else {
        incrementLanguageCount( newObject.lineageEveID );
        }
    
    

    if( ! newObject.isTutorial )        
    logBirth( newObject.id,
              newObject.email,
              newObject.parentID,
              parentEmail,
              ! getFemale( &newObject ),
              newObject.xd,
              newObject.yd,
              players.size(),
              newObject.parentChainLength );
    
    AppLog::infoF( "New player %s connected as player %d (tutorial=%d) (%d,%d)"
                   " (maxPlacementX=%d)",
                   newObject.email, newObject.id,
                   inTutorialNumber, newObject.xs, newObject.ys,
                   maxPlacementX );

    // generate log line whenever a new baby is born
    logFamilyCounts();
    
    return newObject.id;
    }




static void processWaitingTwinConnection( FreshConnection inConnection ) {
    AppLog::infoF( "Player %s waiting for twin party of %d", 
                   inConnection.email,
                   inConnection.twinCount );
    waitingForTwinConnections.push_back( inConnection );
    
    CurseStatus anyTwinCurseLevel = inConnection.curseStatus;
    

    // count how many match twin code from inConnection
    // is this the last one to join the party?
    SimpleVector<FreshConnection*> twinConnections;
    

    for( int i=0; i<waitingForTwinConnections.size(); i++ ) {
        FreshConnection *nextConnection = 
            waitingForTwinConnections.getElement( i );
        
        if( nextConnection->error ) {
            continue;
            }
        
        if( nextConnection->twinCode != NULL
            &&
            strcmp( inConnection.twinCode, nextConnection->twinCode ) == 0 
            &&
            inConnection.twinCount == nextConnection->twinCount ) {
            
            if( strcmp( inConnection.email, nextConnection->email ) == 0 ) {
                // don't count this connection itself
                continue;
                }

            if( nextConnection->curseStatus.curseLevel > 
                anyTwinCurseLevel.curseLevel ) {
                anyTwinCurseLevel = nextConnection->curseStatus;
                }
            
            twinConnections.push_back( nextConnection );
            }
        }

    
    if( twinConnections.size() + 1 >= inConnection.twinCount ) {
        // everyone connected and ready in twin party

        AppLog::infoF( "Found %d other people waiting for twin party of %s, "
                       "ready", 
                       twinConnections.size(), inConnection.email );
        
        char *emailCopy = stringDuplicate( inConnection.email );
        
        // set up twin emails for lineage ban
        for( int i=0; i<twinConnections.size(); i++ ) {
            FreshConnection *nextConnection = 
                twinConnections.getElementDirect( i );
        
            tempTwinEmails.push_back( nextConnection->email );
            }
        
        nextLogInTwin = true;
        
        int newID = processLoggedInPlayer( false,
                                           inConnection.sock,
                                           inConnection.sockBuffer,
                                           inConnection.email,
                                           inConnection.tutorialNumber,
                                           anyTwinCurseLevel,
                                           inConnection.lifeStats,
                                           inConnection.fitnessScore );
        tempTwinEmails.deleteAll();
        
        if( newID == -1 ) {
            AppLog::infoF( "%s reconnected to existing life, not triggering "
                           "fellow twins to spawn now.",
                           emailCopy );

            // take them out of waiting list too
            for( int i=0; i<waitingForTwinConnections.size(); i++ ) {
                if( waitingForTwinConnections.getElement( i )->sock ==
                    inConnection.sock ) {
                    // found
                    
                    waitingForTwinConnections.deleteElement( i );
                    break;
                    }
                }

            delete [] emailCopy;

            if( inConnection.twinCode != NULL ) {
                delete [] inConnection.twinCode;
                inConnection.twinCode = NULL;
                }
            nextLogInTwin = false;
            return;
            }

        delete [] emailCopy;
        
        
        LiveObject *newPlayer = NULL;

        if( inConnection.tutorialNumber == 0 ) {
            newPlayer = getLiveObject( newID );
            }
        else {
            newPlayer = tutorialLoadingPlayers.getElement(
                tutorialLoadingPlayers.size() - 1 );
            }


        int parent = newPlayer->parentID;
        int displayID = newPlayer->displayID;
        GridPos playerPos = { newPlayer->xd, newPlayer->yd };
        
        GridPos *forcedEvePos = NULL;
        
        if( parent == -1 ) {
            // first twin placed was Eve
            // others are identical Eves
            forcedEvePos = &playerPos;
            // trigger forced Eve placement
            parent = -2;
            }


        char usePersonalCurses = 
            SettingsManager::getIntSetting( "usePersonalCurses", 0 );
    


        // save these out here, because newPlayer points into 
        // tutorialLoadingPlayers, which may expand during this loop,
        // invalidating that pointer
        char isTutorial = newPlayer->isTutorial;
        TutorialLoadProgress sharedTutorialLoad = newPlayer->tutorialLoad;

        for( int i=0; i<twinConnections.size(); i++ ) {
            FreshConnection *nextConnection = 
                twinConnections.getElementDirect( i );
            
            processLoggedInPlayer( false, 
                                   nextConnection->sock,
                                   nextConnection->sockBuffer,
                                   nextConnection->email,
                                   // ignore tutorial number of all but
                                   // first player
                                   0,
                                   anyTwinCurseLevel,
                                   nextConnection->lifeStats,
                                   nextConnection->fitnessScore,
                                   parent,
                                   displayID,
                                   forcedEvePos );
            
            // just added is always last object in list
            
            if( usePersonalCurses ) {
                // curse level not known until after first twin logs in
                // their curse level is set based on blockage caused
                // by any of the other twins in the party
                // pass it on.
                LiveObject *newTwinPlayer = 
                    players.getElement( players.size() - 1 );
                newTwinPlayer->curseStatus = newPlayer->curseStatus;
                }



            LiveObject newTwinPlayer = 
                players.getElementDirect( players.size() - 1 );

            if( isTutorial ) {
                // force this one to wait for same tutorial map load
                newTwinPlayer.tutorialLoad = sharedTutorialLoad;

                // flag them as a tutorial player too, so they can't have
                // babies in the tutorial, and they won't be remembered
                // as a long-lineage position at shutdown
                newTwinPlayer.isTutorial = true;

                players.deleteElement( players.size() - 1 );
                
                tutorialLoadingPlayers.push_back( newTwinPlayer );
                }
            }
        

        char *twinCode = stringDuplicate( inConnection.twinCode );
        
        for( int i=0; i<waitingForTwinConnections.size(); i++ ) {
            FreshConnection *nextConnection = 
                waitingForTwinConnections.getElement( i );
            
            if( nextConnection->error ) {
                continue;
                }
            
            if( nextConnection->twinCode != NULL 
                &&
                nextConnection->twinCount == inConnection.twinCount
                &&
                strcmp( nextConnection->twinCode, twinCode ) == 0 ) {
                
                delete [] nextConnection->twinCode;
                waitingForTwinConnections.deleteElement( i );
                i--;
                }
            }
        
        delete [] twinCode;
        
        nextLogInTwin = false;
        }
    }




// doesn't check whether dest itself is blocked
static char directLineBlocked( GridPos inSource, GridPos inDest ) {
    // line algorithm from here
    // https://en.wikipedia.org/wiki/Bresenham's_line_algorithm
    
    double deltaX = inDest.x - inSource.x;
    
    double deltaY = inDest.y - inSource.y;
    

    int xStep = 1;
    if( deltaX < 0 ) {
        xStep = -1;
        }
    
    int yStep = 1;
    if( deltaY < 0 ) {
        yStep = -1;
        }
    

    if( deltaX == 0 ) {
        // vertical line
        
        // just walk through y
        for( int y=inSource.y; y != inDest.y; y += yStep ) {
            if( isMapSpotBlocking( inSource.x, y ) ) {
                return true;
                }
            }
        }
    else {
        double deltaErr = fabs( deltaY / (double)deltaX );
        
        double error = 0;
        
        int y = inSource.y;
        for( int x=inSource.x; x != inDest.x || y != inDest.y; x += xStep ) {
            if( isMapSpotBlocking( x, y ) ) {
                return true;
                }
            error += deltaErr;
            
            if( error >= 0.5 ) {
                y += yStep;
                error -= 1.0;
                }
            
            // we may need to take multiple steps in y
            // if line is vertically oriented
            while( error >= 0.5 ) {
                if( isMapSpotBlocking( x, y ) ) {
                    return true;
                    }

                y += yStep;
                error -= 1.0;
                }
            }
        }

    return false;
    }



char removeFromContainerToHold( LiveObject *inPlayer, 
                                int inContX, int inContY,
                                int inSlotNumber );




// find index of spot on container held item can swap with, or -1 if none found
static int getContainerSwapIndex( LiveObject *inPlayer,
                                  int idToAdd,
                                  int inStillHeld,
                                  int inSearchLimit,
                                  int inContX, int inContY ) {
    // take what's on bottom of container, but only if it's different
    // from what's in our hand
    // AND we are old enough to take it
    double playerAge = computeAge( inPlayer );
    
    // if we find a same object on bottom, keep going up until
    // we find a non-same one to swap
    for( int botInd = 0; botInd < inSearchLimit; botInd ++ ) {
        
        char same = false;
        
        int bottomItem = 
            getContained( inContX, inContY, botInd, 0 );
        
        if( bottomItem > 0 &&
            ! canPickup( bottomItem, playerAge ) ) {
            // too young to hold!
            same = true;
            }
        else if( bottomItem == idToAdd ) {
            if( bottomItem > 0 ) {
                // not sub conts
                same = true;
                }
            else {
                // they must contain same stuff to be same
                int bottomNum = getNumContained( inContX, inContY,
                                                 botInd + 1 );
                int topNum;

                if( inStillHeld ) {
                    topNum = inPlayer->numContained;
                    }
                else {
                    // already in the container
                    topNum =  getNumContained( inContX, inContY,
                                               inSearchLimit + 1 );
                    }
                
                if( bottomNum != topNum ) {
                    same = false;
                    }
                else {
                    same = true;
                    for( int b=0; b<bottomNum; b++ ) {
                        int subB = getContained( inContX, inContY,
                                                 b, botInd + 1 );
                        int subT;

                        if( inStillHeld ) {
                            subT = inPlayer->containedIDs[b];
                            }
                        else {
                            subT = getContained( inContX, inContY,
                                                 b, inSearchLimit + 1 );
                            }
                        
                                
                        if( subB != subT ) {
                            same = false;
                            break;
                            }
                        }
                    }
                }
            }
        if( !same ) {
            return botInd;
            }
        }
    
    return -1;
    }

    
        




// swap indicates that we want to put the held item at the bottom
// of the container and take the top one
// returns true if added
static char addHeldToContainer( LiveObject *inPlayer,
                                int inTargetID,
                                int inContX, int inContY,
                                char inSwap = false ) {
    
    int target = inTargetID;
        
    int targetSlots = 
        getNumContainerSlots( target );
                                        
    ObjectRecord *targetObj =
        getObject( target );
    
    if( isGrave( target ) ) {
        return false;
        }
    if( targetObj->slotsLocked ) {
        return false;
        }

    float slotSize =
        targetObj->slotSize;
    
    float containSize =
        getObject( 
            inPlayer->holdingID )->
        containSize;

    int numIn = 
        getNumContained( inContX, inContY );

    
    int isRoom = false;
    

    if( numIn < targetSlots ) {
        isRoom = true;
        }
    else {
        // container full
        // but check if swap is possible

        if( inSwap ) {
            
            int idToAdd = inPlayer->holdingID;
            TransRecord *r = getPTrans( idToAdd, -1 );

            if( r != NULL && r->newActor == 0 && r->newTarget > 0 ) {
                idToAdd = r->newTarget;
                }
            
            int swapInd = getContainerSwapIndex ( inPlayer,
                                                  idToAdd,
                                                  true,
                                                  numIn,
                                                  inContX, inContY );
            if( swapInd != -1 ) {
                isRoom = true;
                }
            }
        }
    


    
    if( isRoom &&
        isContainable( 
            inPlayer->holdingID ) &&
        containSize <= slotSize ) {
        
        // add to container
        
        setResponsiblePlayer( 
            inPlayer->id );
        

        // adding something to a container acts like a drop
        // but some non-permanent held objects are supposed to go through 
        // a transition when they drop (example:  held wild piglet is
        // non-permanent, so it can be containable, but it supposed to
        // switch to a moving wild piglet when dropped.... we should
        // switch to this other wild piglet when putting it into a container
        // too)

        // "set-down" type bare ground
        // trans exists for what we're 
        // holding?
        TransRecord *r = getPTrans( inPlayer->holdingID, -1 );

        if( r != NULL && r->newActor == 0 && r->newTarget > 0 ) {
                                            
            // only applies if the 
            // bare-ground
            // trans leaves nothing in
            // our hand
            
            // first, change what they
            // are holding to this 
            // newTarget
            

            handleHoldingChange( 
                inPlayer,
                r->newTarget );
            }


        int idToAdd = inPlayer->holdingID;


        float stretch = getObject( idToAdd )->slotTimeStretch;
                    
                    

        if( inPlayer->numContained > 0 ) {
            // negative to indicate sub-container
            idToAdd *= -1;
            }

        

        addContained( 
            inContX, inContY,
            idToAdd,
            inPlayer->holdingEtaDecay );

        if( inPlayer->numContained > 0 ) {
            timeSec_t curTime = Time::timeSec();
            
            for( int c=0; c<inPlayer->numContained; c++ ) {
                
                // undo decay stretch before adding
                // (stretch applied by adding)
                if( stretch != 1.0 &&
                    inPlayer->containedEtaDecays[c] != 0 ) {
                
                    timeSec_t offset = 
                        inPlayer->containedEtaDecays[c] - curTime;
                    
                    offset = offset * stretch;
                    
                    inPlayer->containedEtaDecays[c] = curTime + offset;
                    }


                addContained( inContX, inContY, inPlayer->containedIDs[c],
                              inPlayer->containedEtaDecays[c],
                              numIn + 1 );
                }
            
            clearPlayerHeldContained( inPlayer );
            }
        

        
        setResponsiblePlayer( -1 );
        
        inPlayer->holdingID = 0;
        inPlayer->holdingEtaDecay = 0;
        inPlayer->heldOriginValid = 0;
        inPlayer->heldOriginX = 0;
        inPlayer->heldOriginY = 0;
        inPlayer->heldTransitionSourceID = -1;

        int numInNow = getNumContained( inContX, inContY );
        
        if( inSwap &&  numInNow > 1 ) {
            
            int swapInd = getContainerSwapIndex( inPlayer, 
                                                 idToAdd,
                                                 false,
                                                 // don't consider top slot
                                                 // where we just put this
                                                 // new item
                                                 numInNow - 1,
                                                 inContX, inContY );
            if( swapInd != -1 ) {
                // found one to swap
                removeFromContainerToHold( inPlayer, inContX, inContY, 
                                           swapInd );
                }
            // if we didn't remove one, it means whole container is full
            // of identical items.
            // the swap action doesn't work, so we just let it
            // behave like an add action instead.
            }

        return true;
        }

    return false;
    }



// returns true if succeeded
char removeFromContainerToHold( LiveObject *inPlayer, 
                                int inContX, int inContY,
                                int inSlotNumber ) {
    inPlayer->heldOriginValid = 0;
    inPlayer->heldOriginX = 0;
    inPlayer->heldOriginY = 0;                        
    inPlayer->heldTransitionSourceID = -1;
    

    if( isGridAdjacent( inContX, inContY,
                        inPlayer->xd, 
                        inPlayer->yd ) 
        ||
        ( inContX == inPlayer->xd &&
          inContY == inPlayer->yd ) ) {
                            
        inPlayer->actionAttempt = 1;
        inPlayer->actionTarget.x = inContX;
        inPlayer->actionTarget.y = inContY;
                            
        if( inContX > inPlayer->xd ) {
            inPlayer->facingOverride = 1;
            }
        else if( inContX < inPlayer->xd ) {
            inPlayer->facingOverride = -1;
            }

        // can only use on targets next to us for now,
        // no diags
                            
        int target = getMapObject( inContX, inContY );
                            
        if( target != 0 ) {
                            
            if( target > 0 && getObject( target )->slotsLocked ) {
                return false;
                }

            int numIn = 
                getNumContained( inContX, inContY );
                                
            int toRemoveID = 0;
            
            double playerAge = computeAge( inPlayer );

            
            if( inSlotNumber < 0 ) {
                inSlotNumber = numIn - 1;
                
                // no slot specified
                // find top-most object that they can actually pick up

                int toRemoveID = getContained( 
                    inContX, inContY,
                    inSlotNumber );
                
                if( toRemoveID < 0 ) {
                    toRemoveID *= -1;
                    }
                
                while( inSlotNumber > 0 &&
                       ! canPickup( toRemoveID, playerAge ) )  {
            
                    inSlotNumber--;
                    
                    toRemoveID = getContained( 
                        inContX, inContY,
                        inSlotNumber );
                
                    if( toRemoveID < 0 ) {
                        toRemoveID *= -1;
                        }
                    }
                }
            


                                
            if( numIn > 0 ) {
                toRemoveID = getContained( inContX, inContY, inSlotNumber );
                }
            
            char subContain = false;
            
            if( toRemoveID < 0 ) {
                toRemoveID *= -1;
                subContain = true;
                }

            
            if( toRemoveID == 0 ) {
                // this should never happen, except due to map corruption
                
                // clear container, to be safe
                clearAllContained( inContX, inContY );
                return false;
                }


            if( inPlayer->holdingID == 0 && 
                numIn > 0 &&
                // old enough to handle it
                canPickup( toRemoveID, computeAge( inPlayer ) ) ) {
                // get from container


                if( subContain ) {
                    int subSlotNumber = inSlotNumber;
                    
                    if( subSlotNumber == -1 ) {
                        subSlotNumber = numIn - 1;
                        }

                    inPlayer->containedIDs =
                        getContained( inContX, inContY, 
                                      &( inPlayer->numContained ), 
                                      subSlotNumber + 1 );
                    inPlayer->containedEtaDecays =
                        getContainedEtaDecay( inContX, inContY, 
                                              &( inPlayer->numContained ), 
                                              subSlotNumber + 1 );

                    // these will be cleared when removeContained is called
                    // for this slot below, so just get them now without clearing

                    // empty vectors... there are no sub-sub containers
                    inPlayer->subContainedIDs = 
                        new SimpleVector<int>[ inPlayer->numContained ];
                    inPlayer->subContainedEtaDecays = 
                        new SimpleVector<timeSec_t>[ inPlayer->numContained ];
                
                    }
                
                
                setResponsiblePlayer( - inPlayer->id );
                
                inPlayer->holdingID =
                    removeContained( 
                        inContX, inContY, inSlotNumber,
                        &( inPlayer->holdingEtaDecay ) );
                

                // does bare-hand action apply to this newly-held object
                // one that results in something new in the hand and
                // nothing on the ground?

                // if so, it is a pick-up action, and it should apply here

                TransRecord *pickupTrans = getPTrans( 0, inPlayer->holdingID );
                
                if( pickupTrans != NULL && pickupTrans->newActor > 0 &&
                    pickupTrans->newTarget == 0 ) {
                    
                    handleHoldingChange( inPlayer, pickupTrans->newActor );
                    }
                else {
                    holdingSomethingNew( inPlayer );
                    }
                
                setResponsiblePlayer( -1 );

                if( inPlayer->holdingID < 0 ) {
                    // sub-contained
                    
                    inPlayer->holdingID *= -1;    
                    }
                
                // contained objects aren't animating
                // in a way that needs to be smooth
                // transitioned on client
                inPlayer->heldOriginValid = 0;
                inPlayer->heldOriginX = 0;
                inPlayer->heldOriginY = 0;

                return true;
                }
            }
        }        
    
    return false;
    }



// outCouldHaveGoneIn, if non-NULL, is set to TRUE if clothing
// could potentialy contain what we're holding (even if clothing too full
// to contain it)
static char addHeldToClothingContainer( LiveObject *inPlayer, 
                                        int inC,
                                        // true if we should over-pack
                                        // container in anticipation of a swap
                                        char inWillSwap = false,
                                        char *outCouldHaveGoneIn = NULL ) {    
    // drop into own clothing
    ObjectRecord *cObj = 
        clothingByIndex( 
            inPlayer->clothing,
            inC );
                                    
    if( cObj != NULL &&
        isContainable( 
            inPlayer->holdingID ) ) {
                                        
        int oldNum =
            inPlayer->
            clothingContained[inC].size();
                                        
        float slotSize =
            cObj->slotSize;
                                        
        float containSize =
            getObject( inPlayer->holdingID )->
            containSize;
    
        
        if( containSize <= slotSize &&
            cObj->numSlots > 0 &&
            outCouldHaveGoneIn != NULL ) {
            *outCouldHaveGoneIn = true;
            }

        if( ( oldNum < cObj->numSlots
              || ( oldNum == cObj->numSlots && inWillSwap ) )
            &&
            containSize <= slotSize ) {
            // room (or will swap, so we can over-pack it)
            inPlayer->clothingContained[inC].
                push_back( 
                    inPlayer->holdingID );

            if( inPlayer->
                holdingEtaDecay != 0 ) {
                                                
                timeSec_t curTime = 
                    Time::timeSec();
                                            
                timeSec_t offset = 
                    inPlayer->
                    holdingEtaDecay - 
                    curTime;
                                            
                offset = 
                    offset / 
                    cObj->
                    slotTimeStretch;
                                                
                inPlayer->holdingEtaDecay =
                    curTime + offset;
                }
                                            
            inPlayer->
                clothingContainedEtaDecays[inC].
                push_back( inPlayer->
                           holdingEtaDecay );
                                        
            inPlayer->holdingID = 0;
            inPlayer->holdingEtaDecay = 0;
            inPlayer->heldOriginValid = 0;
            inPlayer->heldOriginX = 0;
            inPlayer->heldOriginY = 0;
            inPlayer->heldTransitionSourceID =
                -1;

            return true;
            }
        }

    return false;
    }



static void setHeldGraveOrigin( LiveObject *inPlayer, int inX, int inY,
                                int inNewTarget ) {
    // make sure that there is nothing left there
    // for now, transitions that remove graves leave nothing behind
    if( inNewTarget == 0 ) {
        
        // make sure that that there was a grave there before
        int gravePlayerID = getGravePlayerID( inX, inY );
        
        // clear it
        inPlayer->heldGravePlayerID = 0;
            

        if( gravePlayerID > 0 ) {
            
            // player action actually picked up this grave
            
            if( inPlayer->holdingID > 0 &&
                strstr( getObject( inPlayer->holdingID )->description, 
                        "origGrave" ) != NULL ) {
                
                inPlayer->heldGraveOriginX = inX;
                inPlayer->heldGraveOriginY = inY;
                
                inPlayer->heldGravePlayerID = getGravePlayerID( inX, inY );
                }
            
            // clear it from ground
            setGravePlayerID( inX, inY, 0 );
            }
        }
    
    }



static void pickupToHold( LiveObject *inPlayer, int inX, int inY, 
                          int inTargetID ) {
    inPlayer->holdingEtaDecay = 
        getEtaDecay( inX, inY );
    
    FullMapContained f =
        getFullMapContained( inX, inY );
    
    setContained( inPlayer, f );
    
    clearAllContained( inX, inY );
    
    setResponsiblePlayer( - inPlayer->id );
    setMapObject( inX, inY, 0 );
    setResponsiblePlayer( -1 );
    
    inPlayer->holdingID = inTargetID;
    holdingSomethingNew( inPlayer );

    setHeldGraveOrigin( inPlayer, inX, inY, 0 );
    
    inPlayer->heldOriginValid = 1;
    inPlayer->heldOriginX = inX;
    inPlayer->heldOriginY = inY;
    inPlayer->heldTransitionSourceID = -1;
    }


// returns true if it worked
static char removeFromClothingContainerToHold( LiveObject *inPlayer,
                                               int inC,
                                               int inI = -1 ) {    
    
    ObjectRecord *cObj = 
        clothingByIndex( inPlayer->clothing, 
                         inC );
                                
    float stretch = 1.0f;
    
    if( cObj != NULL ) {
        stretch = cObj->slotTimeStretch;
        }
    
    int oldNumContained = 
        inPlayer->clothingContained[inC].size();

    int slotToRemove = inI;

    double playerAge = computeAge( inPlayer );

                                
    if( slotToRemove < 0 ) {
        slotToRemove = oldNumContained - 1;

        // no slot specified
        // find top-most object that they can actually pick up

        while( slotToRemove > 0 &&
               ! canPickup( inPlayer->clothingContained[inC].
                            getElementDirect( slotToRemove ), 
                            playerAge ) ) {
            
            slotToRemove --;
            }
        }
                                
    int toRemoveID = -1;
                                
    if( oldNumContained > 0 &&
        oldNumContained > slotToRemove &&
        slotToRemove >= 0 ) {
                                    
        toRemoveID = 
            inPlayer->clothingContained[inC].
            getElementDirect( slotToRemove );
        }

    if( oldNumContained > 0 &&
        oldNumContained > slotToRemove &&
        slotToRemove >= 0 &&
        // old enough to handle it
        canPickup( toRemoveID, playerAge ) ) {
                                    

        inPlayer->holdingID = 
            inPlayer->clothingContained[inC].
            getElementDirect( slotToRemove );
        holdingSomethingNew( inPlayer );

        inPlayer->holdingEtaDecay = 
            inPlayer->
            clothingContainedEtaDecays[inC].
            getElementDirect( slotToRemove );
                                    
        timeSec_t curTime = Time::timeSec();

        if( inPlayer->holdingEtaDecay != 0 ) {
                                        
            timeSec_t offset = 
                inPlayer->holdingEtaDecay
                - curTime;
            offset = offset * stretch;
            inPlayer->holdingEtaDecay =
                curTime + offset;
            }

        inPlayer->clothingContained[inC].
            deleteElement( slotToRemove );
        inPlayer->clothingContainedEtaDecays[inC].
            deleteElement( slotToRemove );

        inPlayer->heldOriginValid = 0;
        inPlayer->heldOriginX = 0;
        inPlayer->heldOriginY = 0;
        inPlayer->heldTransitionSourceID = -1;
        return true;
        }
    
    return false;
    }



static ObjectRecord **getClothingSlot( LiveObject *targetPlayer, int inIndex ) {
    
    ObjectRecord **clothingSlot = NULL;    


    if( inIndex == 2 &&
        targetPlayer->clothing.frontShoe != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.frontShoe );
        }
    else if( inIndex == 3 &&
             targetPlayer->clothing.backShoe 
             != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.backShoe );
        }
    else if( inIndex == 0 && 
             targetPlayer->clothing.hat != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.hat );
        }
    else if( inIndex == 1 &&
             targetPlayer->clothing.tunic 
             != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.tunic );
        }
    else if( inIndex == 4 &&
             targetPlayer->clothing.bottom 
             != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.bottom );
        }
    else if( inIndex == 5 &&
             targetPlayer->
             clothing.backpack != NULL ) {
        clothingSlot = 
            &( targetPlayer->clothing.backpack );
        }
    
    return clothingSlot;
    }

    

static void removeClothingToHold( LiveObject *nextPlayer, 
                                  LiveObject *targetPlayer,
                                  ObjectRecord **clothingSlot,
                                  int clothingSlotIndex ) {
    int ind = clothingSlotIndex;
    
    nextPlayer->holdingID =
        ( *clothingSlot )->id;
    holdingSomethingNew( nextPlayer );
    
    *clothingSlot = NULL;
    nextPlayer->holdingEtaDecay =
        targetPlayer->clothingEtaDecay[ind];
    targetPlayer->clothingEtaDecay[ind] = 0;
    
    nextPlayer->numContained =
        targetPlayer->
        clothingContained[ind].size();
    
    freePlayerContainedArrays( nextPlayer );
    
    nextPlayer->containedIDs =
        targetPlayer->
        clothingContained[ind].
        getElementArray();
    
    targetPlayer->clothingContained[ind].
        deleteAll();
    
    nextPlayer->containedEtaDecays =
        targetPlayer->
        clothingContainedEtaDecays[ind].
        getElementArray();
    
    targetPlayer->
        clothingContainedEtaDecays[ind].
        deleteAll();
    
    // empty sub contained in clothing
    nextPlayer->subContainedIDs =
        new SimpleVector<int>[
            nextPlayer->numContained ];
    
    nextPlayer->subContainedEtaDecays =
        new SimpleVector<timeSec_t>[
            nextPlayer->numContained ];
    
    
    nextPlayer->heldOriginValid = 0;
    nextPlayer->heldOriginX = 0;
    nextPlayer->heldOriginY = 0;
    }



static TransRecord *getBareHandClothingTrans( LiveObject *nextPlayer,
                                              ObjectRecord **clothingSlot ) {
    TransRecord *bareHandClothingTrans = NULL;
    
    if( clothingSlot != NULL ) {
        bareHandClothingTrans =
            getPTrans( 0, ( *clothingSlot )->id );
                                    
        if( bareHandClothingTrans != NULL ) {
            int na =
                bareHandClothingTrans->newActor;
            
            if( na > 0 &&
                ! canPickup( na, computeAge( nextPlayer ) ) ) {
                // too young for trans
                bareHandClothingTrans = NULL;
                }
            
            if( bareHandClothingTrans != NULL ) {
                int nt = 
                    bareHandClothingTrans->
                    newTarget;
                
                if( nt > 0 &&
                    getObject( nt )->clothing 
                    == 'n' ) {
                    // don't allow transitions
                    // that leave a non-wearable
                    // item on your body
                    bareHandClothingTrans = NULL;
                    }
                }
            }
        }
    
    return bareHandClothingTrans;
    }




// change held as the result of a transition
static void handleHoldingChange( LiveObject *inPlayer, int inNewHeldID ) {
    
    LiveObject *nextPlayer = inPlayer;

    int oldHolding = nextPlayer->holdingID;
    
    int oldContained = 
        nextPlayer->numContained;
    
    
    nextPlayer->heldOriginValid = 0;
    nextPlayer->heldOriginX = 0;
    nextPlayer->heldOriginY = 0;
    
    // can newly changed container hold
    // less than what it could contain
    // before?
    
    int newHeldSlots = getNumContainerSlots( inNewHeldID );
    
    if( newHeldSlots < oldContained ) {
        // new container can hold less
        // truncate
                            
        GridPos dropPos = 
            getPlayerPos( inPlayer );
                            
        // offset to counter-act offsets built into
        // drop code
        dropPos.x += 1;
        dropPos.y += 1;
        
        char found = false;
        GridPos spot;
        
        if( getMapObject( dropPos.x, dropPos.y ) == 0 ) {
            spot = dropPos;
            found = true;
            }
        else {
            found = findDropSpot( 
                dropPos.x, dropPos.y,
                dropPos.x, dropPos.y,
                &spot );
            }
        
        
        if( found ) {
            
            // throw it on map temporarily
            handleDrop( 
                spot.x, spot.y, 
                inPlayer,
                // only temporary, don't worry about blocking players
                // with this drop
                NULL );
                                

            // responsible player for stuff thrown on map by shrink
            setResponsiblePlayer( inPlayer->id );

            // shrink contianer on map
            shrinkContainer( spot.x, spot.y, 
                             newHeldSlots );
            
            setResponsiblePlayer( -1 );
            

            // pick it back up
            nextPlayer->holdingEtaDecay = 
                getEtaDecay( spot.x, spot.y );
                                    
            FullMapContained f =
                getFullMapContained( spot.x, spot.y );

            setContained( inPlayer, f );
            
            clearAllContained( spot.x, spot.y );
            setMapObject( spot.x, spot.y, 0 );
            }
        else {
            // no spot to throw it
            // cannot leverage map's container-shrinking
            // just truncate held container directly
            
            // truncated contained items will be lost
            inPlayer->numContained = newHeldSlots;
            }
        }

    nextPlayer->holdingID = inNewHeldID;
    holdingSomethingNew( inPlayer, oldHolding );

    if( newHeldSlots > 0 && 
        oldHolding != 0 ) {
                                        
        restretchDecays( 
            newHeldSlots,
            nextPlayer->containedEtaDecays,
            oldHolding,
            nextPlayer->holdingID );
        }
    
    
    if( oldHolding != inNewHeldID ) {
            
        char kept = false;

        // keep old decay timeer going...
        // if they both decay to the same thing in the same time
        if( oldHolding > 0 && inNewHeldID > 0 ) {
            
            TransRecord *oldDecayT = getMetaTrans( -1, oldHolding );
            TransRecord *newDecayT = getMetaTrans( -1, inNewHeldID );
            
            if( oldDecayT != NULL && newDecayT != NULL ) {
                if( oldDecayT->autoDecaySeconds == newDecayT->autoDecaySeconds
                    && 
                    oldDecayT->newTarget == newDecayT->newTarget ) {
                    
                    kept = true;
                    }
                }
            }
        if( !kept ) {
            setFreshEtaDecayForHeld( nextPlayer );
            }
        }

    }



static unsigned char *makeCompressedMessage( char *inMessage, int inLength,
                                             int *outLength ) {
    
    int compressedSize;
    unsigned char *compressedData =
        zipCompress( (unsigned char*)inMessage, inLength, &compressedSize );



    char *header = autoSprintf( "CM\n%d %d\n#", 
                                inLength,
                                compressedSize );
    int headerLength = strlen( header );
    int fullLength = headerLength + compressedSize;
    
    unsigned char *fullMessage = new unsigned char[ fullLength ];
    
    memcpy( fullMessage, (unsigned char*)header, headerLength );
    
    memcpy( &( fullMessage[ headerLength ] ), compressedData, compressedSize );

    delete [] compressedData;
    
    *outLength = fullLength;
    
    delete [] header;
    
    return fullMessage;
    }



static int maxUncompressedSize = 256;


static void sendMessageToPlayer( LiveObject *inPlayer, 
                                 char *inMessage, int inLength ) {
    if( ! inPlayer->connected ) {
        // stop sending messages to disconnected players
        return;
        }
    
    
    unsigned char *message = (unsigned char*)inMessage;
    int len = inLength;
    
    char deleteMessage = false;

    if( inLength > maxUncompressedSize ) {
        message = makeCompressedMessage( inMessage, inLength, &len );
        deleteMessage = true;
        }

    int numSent = 
        inPlayer->sock->send( message, 
                              len, 
                              false, false );
        
    if( numSent != len ) {
        setPlayerDisconnected( inPlayer, "Socket write failed" );
        }

    inPlayer->gotPartOfThisFrame = true;
    
    if( deleteMessage ) {
        delete [] message;
        }
    }



// result destroyed by caller
static char *getWarReportMessage() {
    SimpleVector<char> workingMessage;
    
    SimpleVector<int> lineageEveIDs;
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( o->error ) {
            continue;
            }
        
        if( lineageEveIDs.getElementIndex( o->lineageEveID ) == -1 ) {
            lineageEveIDs.push_back( o->lineageEveID );
            }
        }

    workingMessage.appendElementString( "WR\n" );

    // check each unique pair of families
    for( int a=0; a<lineageEveIDs.size(); a++ ) {
        int linA = lineageEveIDs.getElementDirect( a );
        for( int b=a+1; b<lineageEveIDs.size(); b++ ) {
            int linB = lineageEveIDs.getElementDirect( b );
            
            char *line = NULL;
            if( isWarState( linA, linB ) ) {
                line = autoSprintf( "%d %d war\n", linA, linB );
                }
            else if( isPeaceTreaty( linA, linB ) ) {
                line = autoSprintf( "%d %d peace\n", linA, linB );
                }
            // no line if neutral
            if( line != NULL ) {
                workingMessage.appendElementString( line );
                delete [] line;
                }
            }
        }

    workingMessage.appendElementString( "#" );

    return workingMessage.getElementString();
    }



void sendWarReportToAll() {
    char *w = getWarReportMessage();
    int len = strlen( w );
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );
        
        if( ! o->error && o->connected ) {
            sendMessageToPlayer( o, w, len );
            }
        }
    delete [] w;
    }



static void sendWarReportToOne( LiveObject *inO ) {
    char *w = getWarReportMessage();
    int len = strlen( w );
    sendMessageToPlayer( inO, w, len );
    delete [] w;
    }
    


void readPhrases( const char *inSettingsName, 
                  SimpleVector<char*> *inList ) {
    char *cont = SettingsManager::getSettingContents( inSettingsName, "" );
    
    if( strcmp( cont, "" ) == 0 ) {
        delete [] cont;
        return;    
        }
    
    int numParts;
    char **parts = split( cont, "\n", &numParts );
    delete [] cont;
    
    for( int i=0; i<numParts; i++ ) {
        if( strcmp( parts[i], "" ) != 0 ) {
            inList->push_back( stringToUpperCase( parts[i] ) );
            }
        delete [] parts[i];
        }
    delete [] parts;
    }



// returns pointer to name in string
char *isNamingSay( char *inSaidString, SimpleVector<char*> *inPhraseList ) {
    char *saidString = inSaidString;
    
    if( saidString[0] == ':' ) {
        // first : indicates reading a written phrase.
        // reading written phrase aloud does not have usual effects
        // (block curse exploit)
        return NULL;
        }
    
    for( int i=0; i<inPhraseList->size(); i++ ) {
        char *testString = inPhraseList->getElementDirect( i );
        
        if( strstr( inSaidString, testString ) == saidString ) {
            // hit
            int phraseLen = strlen( testString );
            // skip spaces after
            while( saidString[ phraseLen ] == ' ' ) {
                phraseLen++;
                }
            return &( saidString[ phraseLen ] );
            }
        }
    return NULL;
    }


// returns newly allocated name, or NULL
// looks for phrases that start with a name
char *isReverseNamingSay( char *inSaidString, 
                          SimpleVector<char*> *inPhraseList ) {
    
    if( inSaidString[0] == ':' ) {
        // first : indicates reading a written phrase.
        // reading written phrase aloud does not have usual effects
        // (block curse exploit)
        return NULL;
        }

    for( int i=0; i<inPhraseList->size(); i++ ) {
        char *testString = inPhraseList->getElementDirect( i );
        
        char *hitLoc = strstr( inSaidString, testString );

        if( hitLoc != NULL ) {

            char *saidDupe = stringDuplicate( inSaidString );

            hitLoc = strstr( saidDupe, testString );

            // back one, to exclude space from name
            if( hitLoc != saidDupe ) {
                hitLoc[-1] = '\0';
                return saidDupe;
                }
            else {
                delete [] saidDupe;
                return NULL;
                }
            }
        }
    return NULL;
    }



char *isBabyNamingSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &nameGivingPhrases );
    }

char *isFamilyNamingSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &familyNameGivingPhrases );
    }

char *isEveNamingSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &eveNameGivingPhrases );
    }

char *isCurseNamingSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &cursingPhrases );
    }

char *isNamedGivingSay( char *inSaidString ) {
    return isReverseNamingSay( inSaidString, &namedGivingPhrases );
    }



static char isWildcardGivingSay( char *inSaidString,
                                 SimpleVector<char*> *inPhrases ) {
    if( inSaidString[0] == ':' ) {
        // first : indicates reading a written phrase.
        // reading written phrase aloud does not have usual effects
        // (block curse exploit)
        return false;
        }

    for( int i=0; i<inPhrases->size(); i++ ) {
        char *testString = inPhrases->getElementDirect( i );
        
        char *hitLoc = strstr( inSaidString, testString );

        if( hitLoc != NULL ) {
            return true;
            }
        }
    return false;
    }



char isYouGivingSay( char *inSaidString ) {
    return isWildcardGivingSay( inSaidString, &youGivingPhrases );
    }

char isFamilyGivingSay( char *inSaidString ) {
    return isWildcardGivingSay( inSaidString, &familyGivingPhrases );
    }

char isOffspringGivingSay( char *inSaidString ) {
    return isWildcardGivingSay( inSaidString, &offspringGivingPhrases );
    }

char isPosseJoiningSay( char *inSaidString ) {
    return isWildcardGivingSay( inSaidString, &posseJoiningPhrases );
    }


char isYouFollowSay( char *inSaidString ) {
    return isWildcardGivingSay( inSaidString, &youFollowPhrases );
    }

// returns pointer into inSaidString
char *isNamedFollowSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &namedFollowPhrases );
    }


char isYouExileSay( char *inSaidString ) {
    return isWildcardGivingSay( inSaidString, &youExilePhrases );
    }

// returns pointer into inSaidString
char *isNamedExileSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &namedExilePhrases );
    }


char isYouRedeemSay( char *inSaidString ) {
    return isWildcardGivingSay( inSaidString, &youRedeemPhrases );
    }

// returns pointer into inSaidString
char *isNamedRedeemSay( char *inSaidString ) {
    return isNamingSay( inSaidString, &namedRedeemPhrases );
    }



char isYouKillSay( char *inSaidString ) {
    return isWildcardGivingSay( inSaidString, &youKillPhrases );
    }


// returns newly allocated string
char *isNamedKillSay( char *inSaidString ) {

    char *name = isReverseNamingSay( inSaidString, &namedAfterKillPhrases );

    if( name != NULL ) {
        return name;
        }
    
    name = isNamingSay( inSaidString, &namedKillPhrases );
    
    if( name != NULL ) {
        return stringDuplicate( name );
        }
    
    return NULL;
    }




LiveObject *getClosestOtherPlayer( LiveObject *inThisPlayer,
                                   double inMinAge = 0,
                                   char inNameMustBeNULL = false ) {
    GridPos thisPos = getPlayerPos( inThisPlayer );

    // don't consider anyone who is too far away
    double closestDist = 20;
    LiveObject *closestOther = NULL;
    
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *otherPlayer = 
            players.getElement(j);
        
        if( otherPlayer != inThisPlayer &&
            ! otherPlayer->error &&
            computeAge( otherPlayer ) >= inMinAge &&
            ( ! inNameMustBeNULL || otherPlayer->name == NULL ) ) {
                                        
            GridPos otherPos = 
                getPlayerPos( otherPlayer );
            
            double dist =
                distance( thisPos, otherPos );
            
            if( dist < closestDist ) {
                closestDist = dist;
                closestOther = otherPlayer;
                }
            }
        }
    return closestOther;
    }



int readIntFromFile( const char *inFileName, int inDefaultValue ) {
    FILE *f = fopen( inFileName, "r" );
    
    if( f == NULL ) {
        return inDefaultValue;
        }
    
    int val = inDefaultValue;
    
    fscanf( f, "%d", &val );

    fclose( f );

    return val;
    }


double killDelayTime = 6.0;


typedef struct KillState {
        int killerID;
        int killerWeaponID;
        int targetID;
        double killStartTime;
        double emotStartTime;
        int emotRefreshSeconds;
    } KillState;


SimpleVector<KillState> activeKillStates;




void apocalypseStep() {
    
    double curTime = Time::getCurrentTime();

    if( !apocalypseTriggered ) {
        
        if( apocalypseRequest == NULL &&
            curTime - lastRemoteApocalypseCheckTime > 
            remoteApocalypseCheckInterval ) {

            lastRemoteApocalypseCheckTime = curTime;

            // don't actually send request to reflector if apocalypse
            // not possible locally
            // or if broadcast mode disabled
            if( SettingsManager::getIntSetting( "remoteReport", 0 ) &&
                SettingsManager::getIntSetting( "apocalypsePossible", 0 ) &&
                SettingsManager::getIntSetting( "apocalypseBroadcast", 0 ) ) {

                printf( "Checking for remote apocalypse\n" );
            
                char *url = autoSprintf( "%s?action=check_apocalypse", 
                                         reflectorURL );
        
                apocalypseRequest =
                    new WebRequest( "GET", url, NULL );
            
                delete [] url;
                }
            }
        else if( apocalypseRequest != NULL ) {
            int result = apocalypseRequest->step();

            if( result == -1 ) {
                AppLog::info( 
                    "Apocalypse check:  Request to reflector failed." );
                }
            else if( result == 1 ) {
                // done, have result

                char *webResult = 
                    apocalypseRequest->getResult();
                
                if( strstr( webResult, "OK" ) == NULL ) {
                    AppLog::infoF( 
                        "Apocalypse check:  Bad response from reflector:  %s.",
                        webResult );
                    }
                else {
                    int newApocalypseNumber = lastApocalypseNumber;
                    
                    sscanf( webResult, "%d\n", &newApocalypseNumber );
                
                    if( newApocalypseNumber > lastApocalypseNumber ) {
                        lastApocalypseNumber = newApocalypseNumber;
                        apocalypseTriggered = true;
                        apocalypseRemote = true;
                        AppLog::infoF( 
                            "Apocalypse check:  New remote apocalypse:  %d.",
                            lastApocalypseNumber );
                        SettingsManager::setSetting( "lastApocalypseNumber",
                                                     lastApocalypseNumber );
                        }
                    }
                    
                delete [] webResult;
                }
            
            if( result != 0 ) {
                delete apocalypseRequest;
                apocalypseRequest = NULL;
                }
            }
        }
        


    if( apocalypseTriggered ) {

        if( !apocalypseStarted ) {
            apocalypsePossible = 
                SettingsManager::getIntSetting( "apocalypsePossible", 0 );

            if( !apocalypsePossible ) {
                // settings change since we last looked at it
                apocalypseTriggered = false;
                return;
                }
            
            AppLog::info( "Apocalypse triggerered, starting it" );

            // restart Eve window, and let this player be the
            // first new Eve
            eveWindowStart = 0;
    
            // reset other apocalypse trigger
            lastBabyPassedThresholdTime = 0;
            
            // repopulate this list later when next Eve window ends
            familyNamesAfterEveWindow.deallocateStringElements();
            familyLineageEveIDsAfterEveWindow.deleteAll();
            familyCountsAfterEveWindow.deleteAll();
            nextBabyFamilyIndex = 0;
            
            if( postWindowFamilyLogFile != NULL ) {
                fclose( postWindowFamilyLogFile );
                postWindowFamilyLogFile = NULL;
                }


            reportArcEnd();
            

            // only broadcast to reflector if apocalypseBroadcast set
            if( !apocalypseRemote &&
                SettingsManager::getIntSetting( "remoteReport", 0 ) &&
                SettingsManager::getIntSetting( "apocalypseBroadcast", 0 ) &&
                apocalypseRequest == NULL && reflectorURL != NULL ) {
                
                AppLog::info( "Apocalypse broadcast set, telling reflector" );

                
                char *reflectorSharedSecret = 
                    SettingsManager::
                    getStringSetting( "reflectorSharedSecret" );
                
                if( reflectorSharedSecret != NULL ) {
                    lastApocalypseNumber++;

                    AppLog::infoF( 
                        "Apocalypse trigger:  New local apocalypse:  %d.",
                        lastApocalypseNumber );

                    SettingsManager::setSetting( "lastApocalypseNumber",
                                                 lastApocalypseNumber );

                    int closestPlayerIndex = -1;
                    double closestDist = 999999999;
                    
                    for( int i=0; i<players.size(); i++ ) {
                        LiveObject *nextPlayer = players.getElement( i );
                        if( !nextPlayer->error ) {
                            
                            double dist = 
                                abs( nextPlayer->xd - apocalypseLocation.x ) +
                                abs( nextPlayer->yd - apocalypseLocation.y );
                            if( dist < closestDist ) {
                                closestPlayerIndex = i;
                                closestDist = dist;
                                }
                            }
                        
                        }
                    char *name = NULL;
                    if( closestPlayerIndex != -1 ) {
                        name = 
                            players.getElement( closestPlayerIndex )->
                            name;
                        }
                    
                    if( name == NULL ) {
                        name = stringDuplicate( "UNKNOWN" );
                        }
                    
                    char *idString = autoSprintf( "%d", lastApocalypseNumber );
                    
                    char *hash = hmac_sha1( reflectorSharedSecret, idString );

                    delete [] idString;

                    char *url = autoSprintf( 
                        "%s?action=trigger_apocalypse"
                        "&id=%d&id_hash=%s&name=%s",
                        reflectorURL, lastApocalypseNumber, hash, name );

                    delete [] hash;
                    delete [] name;
                    
                    printf( "Starting new web request for %s\n", url );
                    
                    apocalypseRequest =
                        new WebRequest( "GET", url, NULL );
                                
                    delete [] url;
                    delete [] reflectorSharedSecret;
                    }
                }


            // send all players the AP message
            const char *message = "AP\n#";
            int messageLength = strlen( message );
            
            for( int i=0; i<players.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( i );
                if( !nextPlayer->error && nextPlayer->connected ) {
                    
                    int numSent = 
                        nextPlayer->sock->send( 
                            (unsigned char*)message, 
                            messageLength,
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != messageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }
                }
            
            apocalypseStartTime = Time::getCurrentTime();
            apocalypseStarted = true;
            postApocalypseStarted = false;
            }

        if( apocalypseRequest != NULL ) {
            
            int result = apocalypseRequest->step();
                

            if( result == -1 ) {
                AppLog::info( 
                    "Apocalypse trigger:  Request to reflector failed." );
                }
            else if( result == 1 ) {
                // done, have result

                char *webResult = 
                    apocalypseRequest->getResult();
                printf( "Apocalypse trigger:  "
                        "Got web result:  '%s'\n", webResult );
                
                if( strstr( webResult, "OK" ) == NULL ) {
                    AppLog::infoF( 
                        "Apocalypse trigger:  "
                        "Bad response from reflector:  %s.",
                        webResult );
                    }
                delete [] webResult;
                }
            
            if( result != 0 ) {
                delete apocalypseRequest;
                apocalypseRequest = NULL;
                }
            }

        if( apocalypseRequest == NULL &&
            Time::getCurrentTime() - apocalypseStartTime >= 8 ) {
            
            if( ! postApocalypseStarted  ) {
                AppLog::infoF( "Enough warning time, %d players still alive",
                               players.size() );
                
                
                double startTime = Time::getCurrentTime();
                
                if( familyDataLogFile != NULL ) {
                    fprintf( familyDataLogFile, "%.2f apocalypse triggered\n",
                             startTime );
                    }
    

                // clear map
                freeMap( true );

                AppLog::infoF( "Apocalypse freeMap took %f sec",
                               Time::getCurrentTime() - startTime );
                wipeMapFiles();

                AppLog::infoF( "Apocalypse wipeMapFiles took %f sec",
                               Time::getCurrentTime() - startTime );
                
                initMap();

                reseedMap( true );
                
                AppLog::infoF( "Apocalypse initMap took %f sec",
                               Time::getCurrentTime() - startTime );
                
                clearTapoutCounts();

                peaceTreaties.deleteAll();
                warStates.deleteAll();
                warPeaceRecords.deleteAll();
                activeKillStates.deleteAll();

                lastRemoteApocalypseCheckTime = curTime;
                
                for( int i=0; i<players.size(); i++ ) {
                    LiveObject *nextPlayer = players.getElement( i );
                    backToBasics( nextPlayer );
                    }
                
                // send everyone update about everyone
                for( int i=0; i<players.size(); i++ ) {
                    LiveObject *nextPlayer = players.getElement( i );
                    if( nextPlayer->connected ) {    
                        nextPlayer->firstMessageSent = false;
                        nextPlayer->firstMapSent = false;
                        nextPlayer->inFlight = false;
                        }
                    // clear monument pos post-apoc
                    // so we don't keep passing the stale info on to
                    // our offspring
                    nextPlayer->monumentPosSet = false;
                    }

                postApocalypseStarted = true;
                }
            else {
                // make sure all players have gotten map and update
                char allMapAndUpdate = true;
                
                for( int i=0; i<players.size(); i++ ) {
                    LiveObject *nextPlayer = players.getElement( i );
                    if( nextPlayer->connected && ! nextPlayer->firstMapSent ) {
                        allMapAndUpdate = false;
                        break;
                        }
                    }
                
                if( allMapAndUpdate ) {
                    
                    // send all players the AD message
                    const char *message = "AD\n#";
                    int messageLength = strlen( message );
            
                    for( int i=0; i<players.size(); i++ ) {
                        LiveObject *nextPlayer = players.getElement( i );
                        if( !nextPlayer->error && nextPlayer->connected ) {
                    
                            int numSent = 
                                nextPlayer->sock->send( 
                                    (unsigned char*)message, 
                                    messageLength,
                                    false, false );
                            
                            nextPlayer->gotPartOfThisFrame = true;
                    
                            if( numSent != messageLength ) {
                                setPlayerDisconnected( nextPlayer, 
                                                       "Socket write failed" );
                                }
                            }
                        }

                    // totally done
                    apocalypseStarted = false;
                    apocalypseTriggered = false;
                    apocalypseRemote = false;
                    postApocalypseStarted = false;
                    }
                }    
            }
        }
    }




void monumentStep() {
    if( monumentCallPending ) {
        
        // send to all players
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            // remember it to tell babies about it
            nextPlayer->monumentPosSet = true;
            nextPlayer->lastMonumentPos.x = monumentCallX;
            nextPlayer->lastMonumentPos.y = monumentCallY;
            nextPlayer->lastMonumentID = monumentCallID;
            nextPlayer->monumentPosSent = true;
            
            if( !nextPlayer->error && nextPlayer->connected ) {
                
                char *message = autoSprintf( "MN\n%d %d %d\n#", 
                                             monumentCallX -
                                             nextPlayer->birthPos.x, 
                                             monumentCallY -
                                             nextPlayer->birthPos.y,
                                             hideIDForClient( 
                                                 monumentCallID ) );
                int messageLength = strlen( message );


                int numSent = 
                    nextPlayer->sock->send( 
                        (unsigned char*)message, 
                        messageLength,
                        false, false );
                
                nextPlayer->gotPartOfThisFrame = true;
                
                delete [] message;

                if( numSent != messageLength ) {
                    setPlayerDisconnected( nextPlayer, "Socket write failed" );
                    }
                }
            }

        monumentCallPending = false;
        }
    }




// inPlayerName may be destroyed inside this function
// returns a uniquified name, sometimes newly allocated.
// return value destroyed by caller
char *getUniqueCursableName( char *inPlayerName, char *outSuffixAdded,
                             char inIsEve, char inFemale ) {
    
    char dup = isNameDuplicateForCurses( inPlayerName );
    
    if( ! dup ) {
        *outSuffixAdded = false;

        if( inIsEve ) {
            // make sure Eve doesn't have same last name as any living person
            char firstName[99];
            char lastName[99];
            
            sscanf( inPlayerName, "%s %s", firstName, lastName );
            
            for( int i=0; i<players.size(); i++ ) {
                LiveObject *o = players.getElement( i );
                
                if( ! o->error && o->familyName != NULL &&
                    strcmp( o->familyName, lastName ) == 0 ) {
                    
                    dup = true;
                    break;
                    }
                }
            }
        

        return inPlayerName;
        }    
    
    
    if( false ) {
        // old code, add suffix to make unique

        *outSuffixAdded = true;

        int targetPersonNumber = 1;
        
        char *fullName = stringDuplicate( inPlayerName );

        while( dup ) {
            // try next roman numeral
            targetPersonNumber++;
            
            int personNumber = targetPersonNumber;            
        
            SimpleVector<char> romanNumeralList;
        
            while( personNumber >= 100 ) {
                romanNumeralList.push_back( 'C' );
                personNumber -= 100;
                }
            while( personNumber >= 50 ) {
                romanNumeralList.push_back( 'L' );
                personNumber -= 50;
                }
            while( personNumber >= 40 ) {
                romanNumeralList.push_back( 'X' );
                romanNumeralList.push_back( 'L' );
                personNumber -= 40;
                }
            while( personNumber >= 10 ) {
                romanNumeralList.push_back( 'X' );
                personNumber -= 10;
                }
            while( personNumber >= 9 ) {
                romanNumeralList.push_back( 'I' );
                romanNumeralList.push_back( 'X' );
                personNumber -= 9;
                }
            while( personNumber >= 5 ) {
                romanNumeralList.push_back( 'V' );
                personNumber -= 5;
                }
            while( personNumber >= 4 ) {
                romanNumeralList.push_back( 'I' );
                romanNumeralList.push_back( 'V' );
                personNumber -= 4;
                }
            while( personNumber >= 1 ) {
                romanNumeralList.push_back( 'I' );
                personNumber -= 1;
                }
            
            char *romanString = romanNumeralList.getElementString();

            delete [] fullName;
            
            fullName = autoSprintf( "%s %s", inPlayerName, romanString );
            delete [] romanString;
            
            dup = isNameDuplicateForCurses( fullName );
            }
        
        delete [] inPlayerName;
        
        return fullName;
        }
    else {
        // new code:
        // make name unique by finding close matching name that hasn't been
        // used recently
        
        *outSuffixAdded = false;

        char firstName[99];
        char lastName[99];
        
        int numNames = sscanf( inPlayerName, "%s %s", firstName, lastName );
        
        if( numNames == 1 ) {
            // special case, find a totally unique first name for them
            
            int i = getFirstNameIndex( firstName, inFemale );

            while( dup ) {

                int nextI;
                
                dup = isNameDuplicateForCurses( getFirstName( i, &nextI, 
                                                              inFemale ) );
            
                if( dup ) {
                    i = nextI;
                    }
                }
            
            if( dup ) {
                // ran out of names, yikes
                return inPlayerName;
                }
            else {
                delete [] inPlayerName;
                int nextI;
                return stringDuplicate( getFirstName( i, &nextI, inFemale ) );
                }
            }
        else if( numNames == 2 ) {
            if( inIsEve ) {
                // cycle last names until we find one not used by any
                // family
                
                int i = getLastNameIndex( lastName );
            
                const char *tempLastName = "";
                
                while( dup ) {
                    
                    int nextI;
                    tempLastName = getLastName( i, &nextI );
                    
                    dup = false;

                    for( int j=0; j<players.size(); j++ ) {
                        LiveObject *o = players.getElement( j );
                        
                        if( ! o->error && 
                            o->familyName != NULL &&
                            strcmp( o->familyName, tempLastName ) == 0 ) {
                    
                            dup = true;
                            break;
                            }
                        }
                    
                    if( dup ) {
                        i = nextI;
                        }
                    }
            
                if( dup ) {
                    // ran out of names, yikes
                    return inPlayerName;
                    }
                else {
                    delete [] inPlayerName;
                    return autoSprintf( "%s %s", firstName, tempLastName );
                    }
                }
            else {
                // cycle first names until we find one
                int i = getFirstNameIndex( firstName, inFemale );
            
                char *tempName = NULL;
                
                while( dup ) {                    
                    if( tempName != NULL ) {
                        delete [] tempName;
                        }
                    
                    int nextI;
                    tempName = autoSprintf( "%s %s", getFirstName( i, &nextI,
                                                                   inFemale ),
                                            lastName );
                    

                    dup = isNameDuplicateForCurses( tempName );
                    if( dup ) {
                        i = nextI;
                        }
                    }
            
                if( dup ) {
                    // ran out of names, yikes
                    if( tempName != NULL ) {
                        delete [] tempName;
                        }
                    return inPlayerName;
                    }
                else {
                    delete [] inPlayerName;
                    return tempName;
                    }
                }
            }
        else {
            // weird case, name doesn't even have two string parts, give up
            return inPlayerName;
            }
        }
    
    }




typedef struct ForcedEffects {
        // -1 if no emot specified
        int emotIndex;
        int ttlSec;
        
        char foodModifierSet;
        double foodCapModifier;
        
        char feverSet;
        float fever;
    } ForcedEffects;
        


ForcedEffects checkForForcedEffects( int inHeldObjectID ) {
    ForcedEffects e = { -1, 0, false, 1.0, false, 0.0f };
    
    ObjectRecord *o = getObject( inHeldObjectID );
    
    if( o != NULL ) {
        char *emotPos = strstr( o->description, "emot_" );
        
        if( emotPos != NULL ) {
            sscanf( emotPos, "emot_%d_%d", 
                    &( e.emotIndex ), &( e.ttlSec ) );
            }

        char *foodPos = strstr( o->description, "food_" );
        
        if( foodPos != NULL ) {
            int numRead = sscanf( foodPos, "food_%lf", 
                                  &( e.foodCapModifier ) );
            if( numRead == 1 ) {
                e.foodModifierSet = true;
                }
            }

        char *feverPos = strstr( o->description, "fever_" );
        
        if( feverPos != NULL ) {
            int numRead = sscanf( feverPos, "fever_%f", 
                                  &( e.fever ) );
            if( numRead == 1 ) {
                e.feverSet = true;
                }
            }
        }
    
    
    return e;
    }




void setNoLongerDying( LiveObject *inPlayer, 
                       SimpleVector<int> *inPlayerIndicesToSendHealingAbout ) {
    inPlayer->dying = false;
    inPlayer->murderSourceID = 0;
    inPlayer->murderPerpID = 0;
    if( inPlayer->murderPerpEmail != 
        NULL ) {
        delete [] 
            inPlayer->murderPerpEmail;
        inPlayer->murderPerpEmail =
            NULL;
        }
    
    inPlayer->deathSourceID = 0;
    inPlayer->holdingWound = false;
    inPlayer->customGraveID = -1;
    
    inPlayer->emotFrozen = false;
    inPlayer->emotUnfreezeETA = 0;
    
    inPlayer->foodCapModifier = 1.0;
    inPlayer->foodUpdate = true;

    inPlayer->fever = 0;

    if( inPlayer->deathReason 
        != NULL ) {
        delete [] inPlayer->deathReason;
        inPlayer->deathReason = NULL;
        }
                                        
    inPlayerIndicesToSendHealingAbout->
        push_back( 
            getLiveObjectIndex( 
                inPlayer->id ) );
    }



static void checkSickStaggerTime( LiveObject *inPlayer ) {
    ObjectRecord *heldObj = NULL;
    
    if( inPlayer->holdingID > 0 ) {
        heldObj = getObject( inPlayer->holdingID );
        }
    else {
        return;
        }

    
    char isSick = false;
    
    if( strstr(
            heldObj->
            description,
            "sick" ) != NULL ) {
        isSick = true;
        
        // sicknesses override basic death-stagger
        // time.  The person can live forever
        // if they are taken care of until
        // the sickness passes
        
        int staggerTime = 
            SettingsManager::getIntSetting(
                "deathStaggerTime", 20 );
        
        double currentTime = 
            Time::getCurrentTime();
        
        // 10x base stagger time should
        // give them enough time to either heal
        // from the disease or die from its
        // side-effects
        inPlayer->dyingETA = 
            currentTime + 10 * staggerTime;
        }
    
    if( isSick ) {
        // what they have will heal on its own 
        // with time.  Sickness, not wound.
        
        // death source is sickness, not
        // source
        inPlayer->deathSourceID = 
            inPlayer->holdingID;
        
        setDeathReason( inPlayer, 
                        "succumbed",
                        inPlayer->holdingID );
        }
    }



typedef struct FlightDest {
        int playerID;
        GridPos destPos;
    } FlightDest;
        



SimpleVector<int> killStatePosseChangedPlayerIDs;


static int countPosseSize( LiveObject *inTarget ) {
    int p = 0;
    
    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
        if( s->targetID == inTarget->id ) {
            p++;
            }
        }
    return p;
    }



static void updatePosseSize( LiveObject *inTarget, 
                             LiveObject *inRemovedKiller = NULL ) {
    
    int p = countPosseSize( inTarget );
    
    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
        
        if( s->targetID == inTarget->id ) {
            int killerID = s->killerID;
            
            LiveObject *o = getLiveObject( killerID );
            
            if( o != NULL ) {
                int oldSize = o->killPosseSize;
                o->killPosseSize = p;
                
                if( oldSize != p ) {
                    killStatePosseChangedPlayerIDs.push_back( killerID );
                    }
                }
            }
        }

    if( inRemovedKiller != NULL ) {
        int oldSize = inRemovedKiller->killPosseSize;
        
        inRemovedKiller->killPosseSize = 0;
        if( oldSize != 0 ) {
            killStatePosseChangedPlayerIDs.push_back( 
                inRemovedKiller->id );
            }
        }
    }



static SimpleVector<int> newEmotPlayerIDs;
static SimpleVector<int> newEmotIndices;
// 0 if no ttl specified
static SimpleVector<int> newEmotTTLs;


static char isNoWaitWeapon( int inObjectID ) {
    return strstr( getObject( inObjectID )->description,
                   "+noWait" ) != NULL;
    }

    


// return true if it worked
char addKillState( LiveObject *inKiller, LiveObject *inTarget,
                   char inInfiniteRange = false ) {
    char found = false;
    
    
    if( ! inInfiniteRange && 
        distance( getPlayerPos( inKiller ), getPlayerPos( inTarget ) )
        > 8 ) {
        // out of range
        return false;
        }
    
    

    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
        
        if( s->killerID == inKiller->id ) {
            found = true;
            s->killerWeaponID = inKiller->holdingID;
            s->targetID = inTarget->id;

            double curTime = Time::getCurrentTime();
            s->emotStartTime = curTime;
            s->killStartTime = curTime;
            
            if( isNoWaitWeapon( inKiller->holdingID ) ) {
                // allow it to happen right now
                s->killStartTime -= killDelayTime;
                }

            s->emotRefreshSeconds = 30;
            break;
            }
        }
    
    if( !found ) {
        // add new
        double curTime = Time::getCurrentTime();
        KillState s = { inKiller->id, 
                        inKiller->holdingID,
                        inTarget->id, 
                        curTime,
                        curTime,
                        30 };
        
        if( isNoWaitWeapon( inKiller->holdingID ) ) {
                // allow it to happen right now
            s.killStartTime -= killDelayTime;
            }

        activeKillStates.push_back( s );

        // force target to gasp
        makePlayerSay( inTarget, (char*)"[GASP]" );
        }

    if( inTarget != NULL ) {
        char *message = autoSprintf( "PJ\n%d %d\n#", 
                                     inKiller->id, inTarget->id );
        sendMessageToPlayer( inTarget, message, strlen( message ) );
        delete [] message;
        }
    
    updatePosseSize( inTarget );
    
    return true;
    }



static void removeKillState( LiveObject *inKiller, LiveObject *inTarget ) {
    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
    
        if( s->killerID == inKiller->id &&
            s->targetID == inTarget->id ) {
            activeKillStates.deleteElement( i );
            
            updatePosseSize( inTarget, inKiller );
            break;
            }
        }

    if( inKiller != NULL ) {
        // clear their emot
        inKiller->emotFrozen = false;
        inKiller->emotUnfreezeETA = 0;
        
        newEmotPlayerIDs.push_back( inKiller->id );
        
        newEmotIndices.push_back( -1 );
        newEmotTTLs.push_back( 0 );
        }

    int newPosseSize = 0;
    if( inTarget != NULL ) {
        newPosseSize = countPosseSize( inTarget );
        }
    
    if( newPosseSize == 0 &&
        inTarget != NULL &&
        inTarget->emotFrozen &&
        inTarget->emotFrozenIndex == victimEmotionIndex ) {
        
        // inTarget's emot hasn't been replaced, end it
        inTarget->emotFrozen = false;
        inTarget->emotUnfreezeETA = 0;
        
        newEmotPlayerIDs.push_back( inTarget->id );
        
        newEmotIndices.push_back( -1 );
        newEmotTTLs.push_back( 0 );
        }

    // killer has left posse
    if( inTarget != NULL ) {
        char *message = autoSprintf( "PJ\n%d 0\n#", 
                                     inKiller->id );
        sendMessageToPlayer( inTarget, message, strlen( message ) );
        delete [] message;
        }
    
    }



static void removeAnyKillState( LiveObject *inKiller ) {
    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
    
        if( s->killerID == inKiller->id ) {
            
            LiveObject *target = getLiveObject( s->targetID );
            
            if( target != NULL ) {
                removeKillState( inKiller, target );
                i--;
                }
            }
        }
    }



static char isAlreadyInKillState( LiveObject *inKiller ) {
    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
    
        if( s->killerID == inKiller->id ) {
            
            LiveObject *target = getLiveObject( s->targetID );
            
            if( target != NULL ) {
                return true;
                }
            }
        }
    return false;
    }

            



static void interruptAnyKillEmots( int inPlayerID, 
                                   int inInterruptingTTL ) {
    for( int i=0; i<activeKillStates.size(); i++ ) {
        KillState *s = activeKillStates.getElement( i );
        
        if( s->killerID == inPlayerID ) {
            s->emotStartTime = Time::getCurrentTime();
            s->emotRefreshSeconds = inInterruptingTTL;
            break;
            }
        }
    }    



static void setPerpetratorHoldingAfterKill( LiveObject *nextPlayer,
                                            TransRecord *woundHit,
                                            TransRecord *rHit,
                                            TransRecord *r ) {

    int oldHolding = nextPlayer->holdingID;


    if( rHit != NULL ) {
        // if hit trans exist
        // leave bloody knife or
        // whatever in hand
        nextPlayer->holdingID = rHit->newActor;
        holdingSomethingNew( nextPlayer,
                             oldHolding );
        }
    else if( woundHit != NULL ) {
        // result of hit on held weapon 
        // could also be
        // specified in wound trans
        nextPlayer->holdingID = 
            woundHit->newActor;
        holdingSomethingNew( nextPlayer,
                             oldHolding );
        }
    else if( r != NULL ) {
        nextPlayer->holdingID = r->newActor;
        holdingSomethingNew( nextPlayer,
                             oldHolding );
        }
                        
    if( r != NULL || rHit != NULL || woundHit != NULL ) {
        
        nextPlayer->heldTransitionSourceID = 0;
        
        if( oldHolding != 
            nextPlayer->holdingID ) {
            
            setFreshEtaDecayForHeld( 
                nextPlayer );
            }
        }
    }



/*
static void printPath( LiveObject *inPlayer ) {
    printf( "Path: " );
    for( int i=0; i<inPlayer->pathLength; i++ ) {
        printf( "(%d,%d) ", inPlayer->pathToDest[i].x,
                inPlayer->pathToDest[i].y );
        }
    printf( "\n" );
    }
*/




void executeKillAction( int inKillerIndex,
                        int inTargetIndex,
                        SimpleVector<int> *playerIndicesToSendUpdatesAbout,
                        SimpleVector<int> *playerIndicesToSendDyingAbout,
                        SimpleVector<int> *newEmotPlayerIDs,
                        SimpleVector<int> *newEmotIndices,
                        SimpleVector<int> *newEmotTTLs ) {
    int i = inKillerIndex;
    LiveObject *nextPlayer = players.getElement( inKillerIndex );    

    LiveObject *hitPlayer = players.getElement( inTargetIndex );

    GridPos targetPos = getPlayerPos( hitPlayer );


    // send update even if action fails (to let them
    // know that action is over)
    playerIndicesToSendUpdatesAbout->push_back( i );
                        
    if( nextPlayer->holdingID > 0 ) {
                            
        nextPlayer->actionAttempt = 1;
        nextPlayer->actionTarget.x = targetPos.x;
        nextPlayer->actionTarget.y = targetPos.y;
                            
        if( nextPlayer->actionTarget.x > nextPlayer->xd ) {
            nextPlayer->facingOverride = 1;
            }
        else if( nextPlayer->actionTarget.x < nextPlayer->xd ) {
            nextPlayer->facingOverride = -1;
            }

        // holding something
        ObjectRecord *heldObj = 
            getObject( nextPlayer->holdingID );
                            
        if( heldObj->deadlyDistance > 0 ) {
            // it's deadly

            GridPos playerPos = getPlayerPos( nextPlayer );
                                
            double d = distance( targetPos,
                                 playerPos );
                                
            if( heldObj->deadlyDistance >= d &&
                ! directLineBlocked( playerPos, 
                                     targetPos ) ) {
                // target is close enough
                // and no blocking objects along the way                

                char someoneHit = false;


                if( hitPlayer != NULL &&
                    strstr( heldObj->description,
                            "otherFamilyOnly" ) ) {
                    // make sure victim is in
                    // different family
                    // and no treaty
                                        
                    if( hitPlayer->lineageEveID ==
                        nextPlayer->lineageEveID
                        || 
                        isPeaceTreaty( hitPlayer->lineageEveID,
                                       nextPlayer->lineageEveID )
                        ||
                        ! isWarState( hitPlayer->lineageEveID,
                                      nextPlayer->lineageEveID ) ) {      
                        hitPlayer = NULL;
                        }
                    }
                

                // special case:
                // non-lethal no_replace ends up in victim's hand
                // they aren't dying, but they may have an emot
                // effect only
                if( hitPlayer != NULL ) {

                    TransRecord *woundHit = 
                        getPTrans( nextPlayer->holdingID, 
                                   0, true, false );

                    if( woundHit != NULL && woundHit->newTarget > 0 &&
                        strstr( getObject( woundHit->newTarget )->description,
                                "no_replace" ) != NULL ) {
                        
                        
                        TransRecord *rHit = 
                            getPTrans( nextPlayer->holdingID, 0, false, true );
                        
                        TransRecord *r = 
                            getPTrans( nextPlayer->holdingID, 0 );

                        setPerpetratorHoldingAfterKill( nextPlayer,
                                                        woundHit, rHit, r );
                        
                        ForcedEffects e = 
                            checkForForcedEffects( woundHit->newTarget );
                            
                        // emote-effect only for no_replace
                        // no fever or food effect
                        if( e.emotIndex != -1 ) {
                            hitPlayer->emotFrozen = 
                                true;
                            hitPlayer->emotFrozenIndex = e.emotIndex;
                            
                            hitPlayer->emotUnfreezeETA =
                                Time::getCurrentTime() + e.ttlSec;
                            
                            newEmotPlayerIDs->push_back( 
                                hitPlayer->id );
                            newEmotIndices->push_back( 
                                e.emotIndex );
                            newEmotTTLs->push_back( 
                                e.ttlSec );

                            interruptAnyKillEmots( hitPlayer->id,
                                                   e.ttlSec );
                            }
                        return;
                        }
                    }
                

                if( hitPlayer != NULL ) {
                    someoneHit = true;
                    // break the connection with 
                    // them, eventually
                    // let them stagger a bit first

                    hitPlayer->murderSourceID =
                        nextPlayer->holdingID;
                                        
                    hitPlayer->murderPerpID =
                        nextPlayer->id;
                                        
                    // brand this player as a murderer
                    nextPlayer->everKilledAnyone = true;

                    if( hitPlayer->murderPerpEmail 
                        != NULL ) {
                        delete [] 
                            hitPlayer->murderPerpEmail;
                        }
                                        
                    hitPlayer->murderPerpEmail =
                        stringDuplicate( 
                            nextPlayer->email );
                                        

                    setDeathReason( hitPlayer, 
                                    "killed",
                                    nextPlayer->holdingID );

                    // if not already dying
                    if( ! hitPlayer->dying ) {
                        int staggerTime = 
                            SettingsManager::getIntSetting(
                                "deathStaggerTime", 20 );
                                            
                        double currentTime = 
                            Time::getCurrentTime();
                                            
                        hitPlayer->dying = true;
                        hitPlayer->dyingETA = 
                            currentTime + staggerTime;

                        playerIndicesToSendDyingAbout->
                            push_back( 
                                getLiveObjectIndex( 
                                    hitPlayer->id ) );
                                        
                        hitPlayer->errorCauseString =
                            "Player killed by other player";
                        }
                    else {
                        // already dying, 
                        // and getting attacked again
                        
                        // halve their remaining 
                        // stagger time
                        double currentTime = 
                            Time::getCurrentTime();
                                             
                        double staggerTimeLeft = 
                            hitPlayer->dyingETA - 
                            currentTime;
                        
                        if( staggerTimeLeft > 0 ) {
                            staggerTimeLeft /= 2;
                            hitPlayer->dyingETA = 
                                currentTime + 
                                staggerTimeLeft;
                            }
                        }
                    }
                                    
                                    
                // a player either hit or not
                // in either case, weapon was used
                                    
                // check for a transition for weapon

                // 0 is generic "on person" target
                TransRecord *r = 
                    getPTrans( nextPlayer->holdingID, 
                               0 );

                TransRecord *rHit = NULL;
                TransRecord *woundHit = NULL;
                                    
                if( someoneHit ) {
                    // last use on target specifies
                    // grave and weapon change on hit
                    // non-last use (r above) specifies
                    // what projectile ends up in grave
                    // or on ground
                    rHit = 
                        getPTrans( nextPlayer->holdingID, 
                                   0, false, true );
                                        
                    if( rHit != NULL &&
                        rHit->newTarget > 0 ) {
                        hitPlayer->customGraveID = 
                            rHit->newTarget;
                        }
                                        
                    char wasSick = false;
                                        
                    if( hitPlayer->holdingID > 0 &&
                        strstr(
                            getObject( 
                                hitPlayer->holdingID )->
                            description,
                            "sick" ) != NULL ) {
                        wasSick = true;
                        }

                    // last use on actor specifies
                    // what is left in victim's hand
                    woundHit = 
                        getPTrans( nextPlayer->holdingID, 
                                   0, true, false );
                                        
                    if( woundHit != NULL &&
                        woundHit->newTarget > 0 ) {
                                            
                        // don't drop their wound
                        if( hitPlayer->holdingID != 0 &&
                            ! hitPlayer->holdingWound &&
                            ! hitPlayer->holdingBiomeSickness ) {
                            handleDrop( 
                                targetPos.x, targetPos.y, 
                                hitPlayer,
                                playerIndicesToSendUpdatesAbout );
                            }

                        // give them a new wound
                        // if they don't already have
                        // one, but never replace their
                        // original wound.  That allows
                        // a healing exploit where you
                        // intentionally give someone
                        // an easier-to-treat wound
                        // to replace their hard-to-treat
                        // wound

                        // however, do let wounds replace
                        // sickness
                        char woundChange = false;
                                            
                        if( ! hitPlayer->holdingWound ||
                            wasSick ) {
                            woundChange = true;
                                                
                            hitPlayer->holdingID = 
                                woundHit->newTarget;
                            holdingSomethingNew( 
                                hitPlayer );
                            setFreshEtaDecayForHeld( 
                                hitPlayer );
                            }
                                            
                                            
                        hitPlayer->holdingWound = true;
                        hitPlayer->holdingBiomeSickness = false;
                        
                        if( woundChange ) {
                                                
                            ForcedEffects e = 
                                checkForForcedEffects( 
                                    hitPlayer->holdingID );
                            
                            if( e.emotIndex != -1 ) {
                                hitPlayer->emotFrozen = 
                                    true;
                                hitPlayer->emotFrozenIndex = e.emotIndex;
                                
                                newEmotPlayerIDs->push_back( 
                                    hitPlayer->id );
                                newEmotIndices->push_back( 
                                    e.emotIndex );
                                newEmotTTLs->push_back( 
                                    e.ttlSec );
                                interruptAnyKillEmots( hitPlayer->id,
                                                       e.ttlSec );
                                }
                                            
                            if( e.foodModifierSet && 
                                e.foodCapModifier != 1 ) {
                                hitPlayer->yummyBonusStore = 0;
                                hitPlayer->
                                    foodCapModifier = 
                                    e.foodCapModifier;
                                hitPlayer->foodUpdate = 
                                    true;
                                }
                                                
                            if( e.feverSet ) {
                                hitPlayer->fever = e.fever;
                                }

                            checkSickStaggerTime( 
                                hitPlayer );
                                                
                            playerIndicesToSendUpdatesAbout->
                                push_back( 
                                    getLiveObjectIndex( 
                                        hitPlayer->id ) );
                            }   
                        }
                    }
                                    

                int oldHolding = nextPlayer->holdingID;

                setPerpetratorHoldingAfterKill( nextPlayer, 
                                                woundHit, rHit, r );

                // if they are moving, end their move NOW
                // (this allows their move speed to get updated
                //  with the murder weapon before their next move)
                // Otherwise, if their move continues, they might walk
                // at the wrong speed with the changed weapon
                

                endAnyMove( nextPlayer );
                

                timeSec_t oldEtaDecay = 
                    nextPlayer->holdingEtaDecay;
                                    

                if( r != NULL ) {
                                    
                    if( hitPlayer != NULL &&
                        r->newTarget != 0 ) {
                                        
                        hitPlayer->embeddedWeaponID = 
                            r->newTarget;
                                        
                        if( oldHolding == r->newTarget ) {
                            // what we are holding
                            // is now embedded in them
                            // keep old decay
                            hitPlayer->
                                embeddedWeaponEtaDecay =
                                oldEtaDecay;
                            }
                        else {
                                            
                            TransRecord *newDecayT = 
                                getMetaTrans( 
                                    -1, 
                                    r->newTarget );
                    
                            if( newDecayT != NULL ) {
                                hitPlayer->
                                    embeddedWeaponEtaDecay = 
                                    Time::timeSec() + 
                                    newDecayT->
                                    autoDecaySeconds;
                                }
                            else {
                                // no further decay
                                hitPlayer->
                                    embeddedWeaponEtaDecay 
                                    = 0;
                                }
                            }
                        }
                    else if( hitPlayer == NULL &&
                             isMapSpotEmpty( targetPos.x, 
                                             targetPos.y ) ) {
                        // this is old code, and probably never gets executed
                        
                        // no player hit, and target ground
                        // spot is empty
                        setMapObject( targetPos.x, targetPos.y, 
                                      r->newTarget );
                                        
                        // if we're thowing a weapon
                        // target is same as what we
                        // were holding
                        if( oldHolding == r->newTarget ) {
                            // preserve old decay time 
                            // of what we were holding
                            setEtaDecay( targetPos.x, targetPos.y,
                                         oldEtaDecay );
                            }
                        }
                    // else new target, post-kill-attempt
                    // is lost
                    }
                }
            }
        }
    }




static void nameEve( LiveObject *nextPlayer, char *name ) {
    
    const char *close = findCloseLastName( name );
    nextPlayer->name = autoSprintf( "%s %s", eveName, close );
    
                                
    nextPlayer->name = getUniqueCursableName( 
        nextPlayer->name, 
        &( nextPlayer->nameHasSuffix ),
        true,
        getFemale( nextPlayer ) );
                                
    char firstName[99];
    char lastName[99];
    char suffix[99];
    
    if( nextPlayer->nameHasSuffix ) {
        
        sscanf( nextPlayer->name, 
                "%s %s %s", 
                firstName, lastName, suffix );
        }
    else {
        sscanf( nextPlayer->name, 
                "%s %s", 
                firstName, lastName );
        }
    
    nextPlayer->familyName = 
        stringDuplicate( lastName );
    
    
    if( ! nextPlayer->isTutorial ) {    
        logName( nextPlayer->id,
                 nextPlayer->email,
                 nextPlayer->name,
                 nextPlayer->lineageEveID );
        }
    }

                                


void nameBaby( LiveObject *inNamer, LiveObject *inBaby, char *inName,
               SimpleVector<int> *playerIndicesToSendNamesAbout ) {    

    LiveObject *nextPlayer = inNamer;
    LiveObject *babyO = inBaby;
    
    char *name = inName;
    

    // NEW:  keep the baby's family name at all costs, even in case
    // of adoption
    // (if baby has no family name, then take mother's family name as last
    // name)
    
    const char *lastName = "";
    

    // note that we skip this case now, in favor of keeping baby's family name
    if( false && nextPlayer->name != NULL ) {
        lastName = strstr( nextPlayer->name, 
                           " " );
                                        
        if( lastName != NULL ) {
            // skip space
            lastName = &( lastName[1] );
            }

        if( lastName == NULL ) {
            lastName = "";

            if( nextPlayer->familyName != 
                NULL ) {
                lastName = 
                    nextPlayer->familyName;
                }    
            }
        else if( nextPlayer->nameHasSuffix ) {
            // only keep last name
            // if it contains another
            // space (the suffix is after
            // the last name).  Otherwise
            // we are probably confused,
            // and what we think
            // is the last name IS the suffix.
                                            
            char *suffixPos =
                strstr( (char*)lastName, " " );
                                            
            if( suffixPos == NULL ) {
                // last name is suffix, actually
                // don't pass suffix on to baby
                lastName = "";
                }
            else {
                // last name plus suffix
                // okay to pass to baby
                // because we strip off
                // third part of name
                // (suffix) below.
                }
            }
        }
    else if( babyO->familyName != NULL ) {
        lastName = babyO->familyName;
        }
    else if( nextPlayer->familyName != NULL ) {
        lastName = nextPlayer->familyName;
        }
                                    


    const char *close = 
        findCloseFirstName( name, getFemale( inBaby ) );

    if( strcmp( lastName, "" ) != 0 ) {    
        babyO->name = autoSprintf( "%s %s",
                                   close, 
                                   lastName );
        }
    else {
        babyO->name = stringDuplicate( close );
        }
    
    
    if( babyO->familyName == NULL &&
        nextPlayer->familyName != NULL ) {
        // mother didn't have a family 
        // name set when baby was born
        // now she does
        // or whatever player named 
        // this orphaned baby does
        babyO->familyName = 
            stringDuplicate( 
                nextPlayer->familyName );
        }
                                    

    int spaceCount = 0;
    int lastSpaceIndex = -1;

    int nameLen = strlen( babyO->name );
    for( int s=0; s<nameLen; s++ ) {
        if( babyO->name[s] == ' ' ) {
            lastSpaceIndex = s;
            spaceCount++;
            }
        }
                                    
    if( spaceCount > 1 ) {
        // remove suffix from end
        babyO->name[ lastSpaceIndex ] = '\0';
        }
                                    
    babyO->name = getUniqueCursableName( 
        babyO->name, 
        &( babyO->nameHasSuffix ), false,
        getFemale( babyO ) );
                                    
    logName( babyO->id,
             babyO->email,
             babyO->name,
             babyO->lineageEveID );
                                    
    playerIndicesToSendNamesAbout->push_back( 
        getLiveObjectIndex( babyO->id ) );
    }



// after person has been named, use this to filter phrase itself
// destroys inSaidPhrase and replaces it
void replaceNameInSaidPhrase( char *inSaidName, char **inSaidPhrase,
                              LiveObject *inNamedPerson, 
                              char inForceBoth = false ) {
    char *trueName;
    if( inForceBoth || strstr( inSaidName, " " ) != NULL ) {
        // multi-word said name
        // assume first and last name
        trueName = stringDuplicate( inNamedPerson->name );
        }
    else {
        // single-word said name
        trueName = stringDuplicate( inNamedPerson->name );
        // trim off last name, if there is one
        char *spacePos = strstr( trueName, " " );
        if( spacePos != NULL ) {
            spacePos[0] = '\0';
            }
        }
    char found = false;
    char *newPhrase = replaceOnce( *inSaidPhrase, inSaidName, trueName,
                                   &found );
    delete [] trueName;
    
    delete [] (*inSaidPhrase);

    *inSaidPhrase = newPhrase;
    }




void getLineageLineForPlayer( LiveObject *inPlayer,
                              SimpleVector<char> *inVector ) {
    
    char *pID = autoSprintf( "%d", inPlayer->id );
    inVector->appendElementString( pID );
    delete [] pID;
    
    for( int j=0; j<inPlayer->lineage->size(); j++ ) {
        char *mID = 
            autoSprintf( 
                " %d",
                inPlayer->lineage->getElementDirect( j ) );
        inVector->appendElementString( mID );
        delete [] mID;
        }        
    // include eve tag at end
    char *eveTag = autoSprintf( " eve=%d",
                                inPlayer->lineageEveID );
    inVector->appendElementString( eveTag );
    delete [] eveTag;
    
    inVector->push_back( '\n' );            
    }



static void endBiomeSickness( 
    LiveObject *nextPlayer,
    int i,
    SimpleVector<int> *playerIndicesToSendUpdatesAbout ) {
    
    int oldSickness = -1;
    
    if( ! nextPlayer->holdingWound ) {
        // back to holding nothing
        oldSickness = nextPlayer->holdingID;
                                        
        nextPlayer->holdingID = 0;
                                        
        playerIndicesToSendUpdatesAbout->
            push_back( i );
        }
                                    
    nextPlayer->holdingBiomeSickness = 
        false;

    // relief emot
    nextPlayer->emotFrozen = false;
    nextPlayer->emotUnfreezeETA = 0;
        
    newEmotPlayerIDs.push_back( 
        nextPlayer->id );
        
    int newEmot = 
        getBiomeReliefEmot( oldSickness );
                                    
    if( newEmot != -1 ) {
        newEmotIndices.push_back( newEmot );
        // 3 sec
        newEmotTTLs.push_back( 3 );
        }
    else {
        // clear
        newEmotIndices.push_back( -1 );
        // 3 sec
        newEmotTTLs.push_back( 0 );
        }
    }






void logFitnessDeath( LiveObject *nextPlayer ) {
    
    // log this death for fitness purposes,
    // for both tutorial and non    


    // if this person themselves died before their mom, gma, etc.
    // remove them from the "ancestor" list of everyone who is older than they
    // are and still alive

    // You only get genetic points for ma, gma, and other older ancestors
    // if you are alive when they die.

    // This ends an exploit where people suicide as a baby (or young person)
    // yet reap genetic benefit from their mother living a long life
    // (your mother, gma, etc count for your genetic score if you yourself
    //  live beyond 3, so it is in your interest to protect them)
    double deadPersonAge = computeAge( nextPlayer );
    if( deadPersonAge < forceDeathAge ) {
        for( int i=0; i<players.size(); i++ ) {
                
            LiveObject *o = players.getElement( i );
            
            if( o->error ||
                o->isTutorial ||
                o->id == nextPlayer->id ) {
                continue;
                }
            
            if( computeAge( o ) < deadPersonAge ) {
                // this person was born after the dead person
                // thus, there's no way they are their ma, gma, etc.
                continue;
                }

            for( int e=0; e< o->ancestorIDs->size(); e++ ) {
                if( o->ancestorIDs->getElementDirect( e ) == nextPlayer->id ) {
                    o->ancestorIDs->deleteElement( e );
                    
                    delete [] o->ancestorEmails->getElementDirect( e );
                    o->ancestorEmails->deleteElement( e );
                
                    delete [] o->ancestorRelNames->getElementDirect( e );
                    o->ancestorRelNames->deleteElement( e );
                    
                    o->ancestorLifeStartTimeSeconds->deleteElement( e );

                    break;
                    }
                }
            }
        }


    SimpleVector<int> emptyAncestorIDs;
    SimpleVector<char*> emptyAncestorEmails;
    SimpleVector<char*> emptyAncestorRelNames;
    SimpleVector<double> emptyAncestorLifeStartTimeSeconds;
    

    SimpleVector<int> *ancestorIDs = nextPlayer->ancestorIDs;
    SimpleVector<char*> *ancestorEmails = nextPlayer->ancestorEmails;
    SimpleVector<char*> *ancestorRelNames = nextPlayer->ancestorRelNames;
    SimpleVector<double> *ancestorLifeStartTimeSeconds = 
        nextPlayer->ancestorLifeStartTimeSeconds;
    

    if( nextPlayer->suicide ) {
        // don't let this suicide death affect scores of any ancestors
        ancestorIDs = &emptyAncestorIDs;
        ancestorEmails = &emptyAncestorEmails;
        ancestorRelNames = &emptyAncestorRelNames;
        ancestorLifeStartTimeSeconds = &emptyAncestorLifeStartTimeSeconds;
        }
    else {
        // any that never made it to age 3+ by the time this person died
        // should not be counted.  What could they have done to keep us alive
        // Note that this misses one case... an older sib that died at age 2.5
        // and then we died at age 10 or whatever.  They are age "12.5" right
        // now, even though they are dead.  We're not still tracking them,
        // though, so we don't know.
        double curTime = Time::getCurrentTime();
        
        double ageRate = getAgeRate();
        
        for( int i=0; i<ancestorEmails->size(); i++ ) {
            double startTime = 
                ancestorLifeStartTimeSeconds->getElementDirect( i );
            
            if( ageRate * ( curTime - startTime ) < defaultActionAge ) {
                // too young to have taken action to help this person
                ancestorIDs->deleteElement( i );
                
                delete [] ancestorEmails->getElementDirect( i );
                ancestorEmails->deleteElement( i );
                
                delete [] ancestorRelNames->getElementDirect( i );
                ancestorRelNames->deleteElement( i );
                
                ancestorLifeStartTimeSeconds->deleteElement( i );
                
                i--;
                }
            }
        
        }    


    logFitnessDeath( players.size(),
                     nextPlayer->email, 
                     nextPlayer->name, nextPlayer->displayID,
                     computeAge( nextPlayer ),
                     ancestorEmails, 
                     ancestorRelNames );
    }




static void logClientTag( FreshConnection *inConnection ) {
    const char *tagToLog = "no_tag";
    
    if( inConnection->clientTag != NULL ) {
        tagToLog = inConnection->clientTag;
        }
    
    FILE *log = fopen( "clientTagLog.txt", "a" );
    
    if( log != NULL ) {
        fprintf( log, "%.0f %s %s\n", Time::getCurrentTime(),
                 inConnection->email, tagToLog );
        
        fclose( log );
        }
    }



static void sendLearnedToolMessage( LiveObject *inPlayer,
                                    SimpleVector<int> *inNewToolSets ) {
    SimpleVector<int> setList;
    
    for( int i=0; i < inNewToolSets->size(); i++ ) {
        getToolSetMembership( inNewToolSets->getElementDirect(i), 
                              &( setList ) );
        }

    // send LR message to let client know that these tools are learned now
    SimpleVector<char> messageWorking;
    
    messageWorking.appendElementString( "LR\n" );
    for( int i=0; i<setList.size(); i++ ) {
        if( i > 0 ) {
            messageWorking.appendElementString( " " );
            }
        char *idString = autoSprintf( "%d", 
                                      setList.getElementDirect( i ) );
        messageWorking.appendElementString( idString );
        delete [] idString;
        }
    messageWorking.appendElementString( "\n#" );
    char *lrMessage = messageWorking.getElementString();
    
    sendMessageToPlayer( inPlayer, lrMessage, strlen( lrMessage ) );
    delete [] lrMessage;
    }


    
    


static char learnTool( LiveObject *inPlayer, int inToolID ) {
    ObjectRecord *toolO = getObject( inToolID );
                                    
    // is it a marked tool?
    int toolSet = toolO->toolSetIndex;
    
    if( toolSet != -1 &&
        inPlayer->learnedTools.getElementIndex( toolSet ) == -1 &&
        inPlayer->numToolSlots > inPlayer->learnedTools.size() ) {
        
        inPlayer->learnedTools.push_back( toolSet );
        
        SimpleVector<int> newToolSets;
        newToolSets.push_back( toolSet );
        
        sendLearnedToolMessage( inPlayer, &newToolSets );
        
        
        // now send DING message
        const char *article = "THE ";
        
        char *des = stringToUpperCase( toolO->description );


        
        // if it's a group of tools, like +toolSterile_Technique
        // show the group name instead of the individual tool
        
        char *toolPos = strstr( des, "+TOOL" );
        
        if( toolPos != NULL ) {
            char *tagPos = &( toolPos[5] );
            
            if( tagPos[0] != '\0' && tagPos[0] != ' ' ) {
                int tagLen = strlen( tagPos );
                for( int i=0; i<tagLen; i++ ) {
                    if( tagPos[i] == ' ' ) {
                        tagPos[i] = '\0';
                        break;
                        }
                    }
                // now replace any _ with ' '
                tagLen = strlen( tagPos );
                for( int i=0; i<tagLen; i++ ) {
                    if( tagPos[i] == '_' ) {
                        tagPos[i] = ' ';
                        }
                    }
                char *newDes = stringDuplicate( tagPos );
                delete [] des;
                des = newDes;
                }
            }
        
        
        stripDescriptionComment( des );

        int desLen = strlen( des );
        if( ( desLen > 0 && des[ desLen - 1 ] == 'S' ) ||
            ( desLen > 2 && des[ desLen - 1 ] == 'G'
              && des[ desLen - 2 ] == 'N' 
              && des[ desLen - 3 ] == 'I' ) ) {
            // use THE for singular tools like YOU LEARNED THE AXE
            // no article for plural tools like YOU LEARNED KNITTING NEEDLES
            // no article for activities (usually tool groups) like SEWING
            article = "";
            }

        char *message;
        
        int numLeft = inPlayer->numToolSlots - inPlayer->learnedTools.size();
        
        if( numLeft > 0 ) {
            message = autoSprintf( "YOU LEARNED %s%s.**"
                                   "%d OF %d TOOL SLOTS ARE LEFT.", 
                                   article, des,
                                   numLeft,
                                   inPlayer->numToolSlots );
            }
        else {
            message = autoSprintf( "YOU LEARNED %s%s.**"
                                   "ALL OF YOUR TOOL SLOTS HAVE BEEN USED.", 
                                   article, des );            
            }
        
        sendGlobalMessage( message, inPlayer );
        
        delete [] des;
        delete [] message;


        return true;
        }
    return false;
    }


static char canPlayerUseOrLearnTool( LiveObject *inPlayer, int inToolID ) {
    if( ! canPlayerUseTool( inPlayer, inToolID ) ) {
        return learnTool( inPlayer, inToolID );
        }
    return true;
    }



static char isBiomeAllowedForPlayer( LiveObject *inPlayer, int inX, int inY ) {
    if( inPlayer->vogMode ||
        inPlayer->forceSpawn ||
        inPlayer->isTutorial ) {
        return true;
        }

    if( inPlayer->holdingID > 0 ) {
        ObjectRecord *heldO = getObject( inPlayer->holdingID );
        if( heldO->permanent &&
            heldO->speedMult == 0 ) {
            // what they're holding is stuck stuck stuck, and they can't
            // move at all.
            
            // is there some way for them to drop it?
            // this prevents us from mistakenly dropping wounds that
            // don't let you move or whatever
            TransRecord *bareGroundT = getPTrans( inPlayer->holdingID, -1 );
            
            if( bareGroundT != NULL && bareGroundT->newTarget > 0 ) {
                // Don't block them from dropping this object
                return true;
                }
            }
        }

    return isBiomeAllowed( inPlayer->displayID, inX, inY );
    }




static char heldNeverDrop( LiveObject *inPlayer ) {
    if( inPlayer->holdingID > 0 ) {        
        ObjectRecord *o = getObject( inPlayer->holdingID );
        if( strstr( o->description, "+neverDrop" ) != NULL ) {
            return true;
            }
        }
    return false;
    }

    


// access blocked b/c of access direction or ownership?
static char isAccessBlocked( LiveObject *inPlayer, 
                             int inTargetX, int inTargetY,
                             int inTargetID ) {
    int target = inTargetID;
    
    int x = inTargetX;
    int y = inTargetY;
    

    char wrongSide = false;
    char ownershipBlocked = false;
    
    if( target > 0 ) {
        ObjectRecord *targetObj = getObject( target );

        if( isGridAdjacent( x, y,
                            inPlayer->xd, 
                            inPlayer->yd ) ) {
            
            if( targetObj->sideAccess ) {
                
                if( y > inPlayer->yd ||
                    y < inPlayer->yd ) {
                    // access from N or S
                    wrongSide = true;
                    }
                }
            else if( targetObj->noBackAccess ) {
                if( y < inPlayer->yd ) {
                    // access from N
                    wrongSide = true;
                    }
                }
            }
        if( targetObj->isOwned ) {
            // make sure player owns this pos
            ownershipBlocked = 
                ! isOwned( inPlayer, x, y );
            }
        }
    return wrongSide || ownershipBlocked;
    }



// cost set to 0 unless hungry work not blocked
char isHungryWorkBlocked( LiveObject *inPlayer, 
                          int inNewTarget, int *outCost ) {          
    *outCost = 0;
    
    char *des =
        getObject( inNewTarget )->description;
                                    
    char *desPos =
        strstr( des, "+hungryWork" );
    
    if( desPos != NULL ) {
                                        
        int cost = 0;
        
        sscanf( desPos,
                "+hungryWork%d", 
                &cost );
        
        if( inPlayer->foodStore + 
            inPlayer->yummyBonusStore < 
            cost + 4 ) {
            // block hungry work,
            // not enough food to have a
            // "safe" buffer after
            return true;
            }
        
        // can do work
        *outCost = cost;
        return false;
        }

    // not hungry work at all
    return false;
    }



// returns NULL if not found
static LiveObject *getPlayerByName( char *inName, LiveObject *inSkip ) {
    for( int j=0; j<players.size(); j++ ) {
        LiveObject *otherPlayer = players.getElement( j );
        if( ! otherPlayer->error &&
            otherPlayer != inSkip &&
            otherPlayer->name != NULL &&
            strcmp( otherPlayer->name, inName ) == 0 ) {
            
            return otherPlayer;
            }
        }
    return NULL;
    }




// if inAll, generates info for all players, and doesn't touch 
//           followingUpdate flags
// returns NULL if no following message
static unsigned char *getFollowingMessage( char inAll, int *outLength ) {
    unsigned char *followingMessage = NULL;
    int followingMessageLength = 0;
        
    SimpleVector<char> followingWorking;
    followingWorking.appendElementString( "FW\n" );
            
    int numAdded = 0;
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *nextPlayer = players.getElement( i );
        if( nextPlayer->error ) {
            continue;
            }
        
        if( nextPlayer->followingUpdate || inAll ) {

            int colorIndex = -1;
            
            if( nextPlayer->followingID > 0 ) {
                LiveObject *l = getLiveObject( nextPlayer->followingID );
                
                if( l != NULL ) {
                    colorIndex = l->leadingColorIndex;
                    }
                }

            char *line = autoSprintf( "%d %d %d\n", 
                                      nextPlayer->id,
                                      nextPlayer->followingID,
                                      colorIndex );
                
            followingWorking.appendElementString( line );
            delete [] line;
            numAdded++;

            if( ! inAll ) {
                nextPlayer->followingUpdate = false;
                }
            }
        }
            
    if( numAdded > 0 ) {
        followingWorking.push_back( '#' );
            
        if( numAdded > 0 ) {

            char *followingMessageText = 
                followingWorking.getElementString();
                
            followingMessageLength = strlen( followingMessageText );
                
            if( followingMessageLength < maxUncompressedSize ) {
                followingMessage = (unsigned char*)followingMessageText;
                }
            else {
                // compress for all players once here
                followingMessage = makeCompressedMessage( 
                    followingMessageText, 
                    followingMessageLength, &followingMessageLength );
                    
                delete [] followingMessageText;
                }
            }
        }

    *outLength = followingMessageLength;
    return followingMessage;
    }



// if inAll, generates info for all players, and doesn't touch exileUpdate flags
// returns NULL if no exile message
static unsigned char *getExileMessage( char inAll, int *outLength ) {
    unsigned char *exileMessage = NULL;
    int exileMessageLength = 0;
    

    SimpleVector<char> exileWorking;
    exileWorking.appendElementString( "EX\n" );
    
    int numAdded = 0;
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *nextPlayer = players.getElement( i );
        if( nextPlayer->error ) {
            continue;
            }
        if( nextPlayer->exileUpdate || inAll ) {

            if( nextPlayer->exiledByIDs.size() > 0 ||
                ( !inAll && nextPlayer->exileUpdate ) ) {
                // send preface line for this player
                // they have some lines coming OR we have a force-update
                // for a player with no exile status (client-side list should
                // be cleared)
                char *line = autoSprintf( "%d -1\n", nextPlayer->id  );
                
                exileWorking.appendElementString( line );
                delete [] line;
                numAdded++;
                }
            
            for( int e=0; e< nextPlayer->exiledByIDs.size(); e++ ) {
                
                char *line = autoSprintf( 
                    "%d %d\n", 
                    nextPlayer->id,
                    nextPlayer->exiledByIDs.getElementDirect( e ) );
                
                exileWorking.appendElementString( line );
                delete [] line;
                numAdded++;
                }

            if( ! inAll ) {
                nextPlayer->exileUpdate = false;
                }
            }
        }
    
    if( numAdded > 0 ) {
        exileWorking.push_back( '#' );
        
        if( numAdded > 0 ) {
            
            char *exileMessageText = 
                exileWorking.getElementString();
            
            exileMessageLength = strlen( exileMessageText );
            
            if( exileMessageLength < maxUncompressedSize ) {
                exileMessage = (unsigned char*)exileMessageText;
                }
            else {
                // compress for all players once here
                exileMessage = makeCompressedMessage( 
                    exileMessageText, 
                    exileMessageLength, &exileMessageLength );
                
                delete [] exileMessageText;
                }
            }
        }

    *outLength = exileMessageLength;
    return exileMessage;
    }



// Recursively walks up leader tree to find out if inLeader is a leader
static char isFollower( LiveObject *inLeader, LiveObject *inTestFollower ) {
    int nextID = inTestFollower->followingID;
    
    if( nextID > 0 ) {
        if( nextID == inLeader->id ) {
            return true;
            }

        LiveObject *next = getLiveObject( nextID );
        
        if( next == NULL ) {
            return false;
            }
        return isFollower( inLeader, next );
        }
    return false;
    }
    


// any followers switch to following the leader of this leader
// exiles are passed down to followers
static void leaderDied( LiveObject *inLeader ) {

    SimpleVector<LiveObject*> exiledByThisLeader;
    
    for( int i=0; i<players.size(); i++ ) {
        
        LiveObject *otherPlayer = players.getElement( i );
        
        if( otherPlayer != inLeader &&
            ! otherPlayer->error ) {
            
            int exileIndex = otherPlayer->
                exiledByIDs.getElementIndex( inLeader->id );
            
            if( exileIndex != -1 ) {
                
                // we have this other exiled
                exiledByThisLeader.push_back( otherPlayer );
                
                // take ourselves off their list, we're dead
                otherPlayer->exiledByIDs.deleteElement( exileIndex );
                otherPlayer->exileUpdate = true;
                }
            }
        }
    
        
    for( int i=0; i<players.size(); i++ ) {
        
        LiveObject *otherPlayer = players.getElement( i );
        
        if( otherPlayer != inLeader &&
            ! otherPlayer->error ) {
            
            if( otherPlayer->followingID == inLeader->id ) {
                // they were following us

                
                // now they follow our leader
                // (or no leader, if we had none)
                otherPlayer->followingID = inLeader->followingID;
                otherPlayer->followingUpdate = true;
                
                int oID = otherPlayer->id;
                
                // have them exile whoever we were exiling
                for( int e=0; e<exiledByThisLeader.size(); e++ ) {
                    LiveObject *eO = 
                        exiledByThisLeader.getElementDirect( e );
                    
                    if( eO != NULL &&
                        // never have them exile themselves
                        eO->id != oID &&
                        eO->exiledByIDs.getElementIndex( oID ) == -1 ) {
                        // this follower is not already exiling this person
                        eO->exiledByIDs.push_back( oID );
                        eO->exileUpdate = true;
                        }
                    }
                } 
            }
        }
    
    }




static void tryToStartKill( LiveObject *nextPlayer, int inTargetID ) {
    if( inTargetID > 0 && 
        nextPlayer->holdingID > 0 &&
        canPlayerUseOrLearnTool( nextPlayer,
                                 nextPlayer->holdingID ) ) {
                            
        ObjectRecord *heldObj = 
            getObject( nextPlayer->holdingID );
                            
                            
        if( heldObj->deadlyDistance > 0 ) {
            
            // player transitioning into kill state?
                            
            LiveObject *targetPlayer =
                getLiveObject( inTargetID );
                            
            if( targetPlayer != NULL ) {
                                    
                // block intra-family kills with
                // otherFamilyOnly weapons
                char weaponBlocked = false;
                                    
                if( strstr( heldObj->description,
                            "otherFamilyOnly" ) ) {
                    // make sure victim is in
                    // different family
                    // AND that there's no peace treaty
                    if( targetPlayer->lineageEveID ==
                        nextPlayer->lineageEveID
                        ||
                        isPeaceTreaty( 
                            targetPlayer->lineageEveID,
                            nextPlayer->lineageEveID )
                        ||
                        ! isWarState( 
                            targetPlayer->lineageEveID,
                            nextPlayer->lineageEveID ) ) {
                                            
                        weaponBlocked = true;
                        }
                    }
                                    
                if( ! weaponBlocked  &&
                    ! isAlreadyInKillState( nextPlayer ) ) {
                    // they aren't already in one
                                        
                    removeAnyKillState( nextPlayer );
                                        
                    char enteredState =
                        addKillState( nextPlayer,
                                      targetPlayer );
                                        
                    if( enteredState && 
                        ! isNoWaitWeapon( 
                            nextPlayer->holdingID ) ) {
                                            
                        // no killer emote for no-wait
                        // weapons (these aren't
                        // actually weapons, like
                        // tattoo needles and snowballs)

                        nextPlayer->emotFrozen = true;
                        nextPlayer->emotFrozenIndex = 
                            killEmotionIndex;
                                            
                        newEmotPlayerIDs.push_back( 
                            nextPlayer->id );
                        newEmotIndices.push_back( 
                            killEmotionIndex );
                        newEmotTTLs.push_back( 120 );
                                            
                        if( ! targetPlayer->emotFrozen ) {
                                                
                            targetPlayer->emotFrozen = true;
                            targetPlayer->emotFrozenIndex =
                                victimEmotionIndex;
                                                
                            newEmotPlayerIDs.push_back( 
                                targetPlayer->id );
                            newEmotIndices.push_back( 
                                victimEmotionIndex );
                            newEmotTTLs.push_back( 120 );
                            }
                        }
                    }
                }
            }
        }
    }



static int getUnusedLeadershipColor() {
    // look for next unused

    int usedCounts[ NUM_BADGE_COLORS ];
    memset( usedCounts, 0, NUM_BADGE_COLORS * sizeof( int ) );
    
    for( int i=0; i<players.size(); i++ ) {
        LiveObject *o = players.getElement( i );

        if( o->leadingColorIndex != -1 ) {
            usedCounts[ o->leadingColorIndex ] ++;
            }
        }
    
    int minUsedCount = players.size();
    int minUsedIndex = -1;
    
    for( int c=0; c<NUM_BADGE_COLORS; c++ ) {
        if( usedCounts[c] < minUsedCount ) {
            minUsedCount = usedCounts[c];
            minUsedIndex = c;
            }
        }

    return minUsedIndex;
    }





int main() {

    if( checkReadOnly() ) {
        printf( "File system read-only.  Server exiting.\n" );
        return 1;
        }
    
    familyDataLogFile = fopen( "familyDataLog.txt", "a" );

    if( familyDataLogFile != NULL ) {
        fprintf( familyDataLogFile, "%.2f server starting up\n",
                 Time::getCurrentTime() );
        }


    memset( allowedSayCharMap, false, 256 );
    
    int numAllowed = strlen( allowedSayChars );
    for( int i=0; i<numAllowed; i++ ) {
        allowedSayCharMap[ (int)( allowedSayChars[i] ) ] = true;
        }
    

    nextID = 
        SettingsManager::getIntSetting( "nextPlayerID", 2 );


    // make backup and delete old backup every day
    AppLog::setLog( new FileLog( "log.txt", 86400 ) );

    AppLog::setLoggingLevel( Log::DETAIL_LEVEL );
    AppLog::printAllMessages( true );

    printf( "\n" );
    AppLog::info( "Server starting up" );

    printf( "\n" );
    
    
    

    nextSequenceNumber = 
        SettingsManager::getIntSetting( "sequenceNumber", 1 );

    requireClientPassword =
        SettingsManager::getIntSetting( "requireClientPassword", 1 );
    
    requireTicketServerCheck =
        SettingsManager::getIntSetting( "requireTicketServerCheck", 1 );
    
    clientPassword = 
        SettingsManager::getStringSetting( "clientPassword" );


    int dataVer = readIntFromFile( "dataVersionNumber.txt", 1 );
    int codVer = readIntFromFile( "serverCodeVersionNumber.txt", 1 );
    
    versionNumber = dataVer;
    if( codVer > versionNumber ) {
        versionNumber = codVer;
        }
    
    printf( "\n" );
    AppLog::infoF( "Server using version number %d", versionNumber );

    printf( "\n" );
    



    minFoodDecrementSeconds = 
        SettingsManager::getFloatSetting( "minFoodDecrementSeconds", 5.0f );

    maxFoodDecrementSeconds = 
        SettingsManager::getFloatSetting( "maxFoodDecrementSeconds", 20 );

    foodScaleFactor = 
        SettingsManager::getFloatSetting( "foodScaleFactor", 1.0 );

    babyBirthFoodDecrement = 
        SettingsManager::getIntSetting( "babyBirthFoodDecrement", 10 );

    indoorFoodDecrementSecondsBonus = SettingsManager::getFloatSetting( 
        "indoorFoodDecrementSecondsBonus", 20 );


    eatBonus = 
        SettingsManager::getIntSetting( "eatBonus", 0 );


    secondsPerYear = 
        SettingsManager::getFloatSetting( "secondsPerYear", 60.0f );
    

    if( clientPassword == NULL ) {
        requireClientPassword = 0;
        }


    ticketServerURL = 
        SettingsManager::getStringSetting( "ticketServerURL" );
    

    if( ticketServerURL == NULL ) {
        requireTicketServerCheck = 0;
        }

    
    reflectorURL = SettingsManager::getStringSetting( "reflectorURL" );

    apocalypsePossible = 
        SettingsManager::getIntSetting( "apocalypsePossible", 0 );

    lastApocalypseNumber = 
        SettingsManager::getIntSetting( "lastApocalypseNumber", 0 );


    childSameRaceLikelihood =
        (double)SettingsManager::getFloatSetting( "childSameRaceLikelihood",
                                                  0.90 );
    
    familySpan =
        SettingsManager::getIntSetting( "familySpan", 2 );

    eveName = 
        SettingsManager::getStringSetting( "eveName", "EVE" );
    
    
    readPhrases( "babyNamingPhrases", &nameGivingPhrases );
    readPhrases( "familyNamingPhrases", &familyNameGivingPhrases );

    readPhrases( "babyNamingPhrases", &eveNameGivingPhrases );

    // add YOU ARE EVE SMITH versions of these
    // put them in front
    SimpleVector<char*> oldPhrases( eveNameGivingPhrases.size() * 2 );
    oldPhrases.push_back_other( &eveNameGivingPhrases );
    eveNameGivingPhrases.deleteAll();
    int numEvePhrases = oldPhrases.size();
    for( int i=0; i<numEvePhrases; i++ ) {
        char *phrase = oldPhrases.getElementDirect( i );
        
        char *newPhrase = autoSprintf( "%s %s", phrase, eveName );
        eveNameGivingPhrases.push_back( newPhrase );
        }
    eveNameGivingPhrases.push_back_other( &oldPhrases );
    

    readPhrases( "cursingPhrases", &cursingPhrases );

    
    readPhrases( "youGivingPhrases", &youGivingPhrases );
    readPhrases( "namedGivingPhrases", &namedGivingPhrases );

    readPhrases( "familyGivingPhrases", &familyGivingPhrases );
    readPhrases( "offspringGivingPhrases", &offspringGivingPhrases );


    readPhrases( "posseJoiningPhrases", &posseJoiningPhrases );


    readPhrases( "youFollowPhrases", &youFollowPhrases );
    readPhrases( "namedFollowPhrases", &namedFollowPhrases );

    readPhrases( "youExilePhrases", &youExilePhrases );
    readPhrases( "namedExilePhrases", &namedExilePhrases );

    readPhrases( "youRedeemPhrases", &youRedeemPhrases );
    readPhrases( "namedRedeemPhrases", &namedRedeemPhrases );


    readPhrases( "youKillPhrases", &youKillPhrases );
    readPhrases( "namedKillPhrases", &namedKillPhrases );
    readPhrases( "namedAfterKillPhrases", &namedAfterKillPhrases );

    
    curseYouPhrase = 
        SettingsManager::getSettingContents( "curseYouPhrase", 
                                             "CURSE YOU" );
    
    curseBabyPhrase = 
        SettingsManager::getSettingContents( "curseBabyPhrase", 
                                             "CURSE MY BABY" );



    
    killEmotionIndex =
        SettingsManager::getIntSetting( "killEmotionIndex", 2 );

    victimEmotionIndex =
        SettingsManager::getIntSetting( "victimEmotionIndex", 2 );
    

#ifdef WIN_32
    printf( "\n\nPress CTRL-C to shut down server gracefully\n\n" );

    SetConsoleCtrlHandler( ctrlHandler, TRUE );
#else
    printf( "\n\nPress CTRL-Z to shut down server gracefully\n\n" );

    signal( SIGTSTP, intHandler );
#endif

    initNames();

    initCurses();
    
    initLifeTokens();
    
    initFitnessScore();
    

    initLifeLog();
    //initBackup();
    
    initPlayerStats();
    initLineageLog();
    
    initLineageLimit();
    
    initCurseDB();
    


    char rebuilding;

    initAnimationBankStart( &rebuilding );
    while( initAnimationBankStep() < 1.0 );
    initAnimationBankFinish();


    initObjectBankStart( &rebuilding, true, true );
    while( initObjectBankStep() < 1.0 );
    initObjectBankFinish();

    
    initCategoryBankStart( &rebuilding );
    while( initCategoryBankStep() < 1.0 );
    initCategoryBankFinish();


    // auto-generate category-based transitions
    initTransBankStart( &rebuilding, true, true, true, true );
    while( initTransBankStep() < 1.0 );
    initTransBankFinish();
    

    // defaults to one hour
    int epochSeconds = 
        SettingsManager::getIntSetting( "epochSeconds", 3600 );
    
    setTransitionEpoch( epochSeconds );


    initFoodLog();
    initFailureLog();

    initObjectSurvey();
    
    initLanguage();
    initFamilySkipList();
    
    
    initTriggers();

    initSpecialBiomes();
    


    if( initMap() != true ) {
        // cannot continue after map init fails
        return 1;
        }
    


    if( false ) {
        
        printf( "Running map sampling\n" );
    
        int idA = 290;
        int idB = 942;
        
        int totalCountA = 0;
        int totalCountB = 0;
        int numRuns = 2;

        for( int i=0; i<numRuns; i++ ) {
        
        
            int countA = 0;
            int countB = 0;
        
            int x = randSource.getRandomBoundedInt( 10000, 300000 );
            int y = randSource.getRandomBoundedInt( 10000, 300000 );
        
            printf( "Sampling at %d,%d\n", x, y );


            for( int yd=y; yd<y + 2400; yd++ ) {
                for( int xd=x; xd<x + 2400; xd++ ) {
                    int oID = getMapObject( xd, yd );
                
                    if( oID == idA ) {
                        countA ++;
                        }
                    else if( oID == idB ) {
                        countB ++;
                        }
                    }
                }
            printf( "   Count at %d,%d is %d = %d, %d = %d\n",
                    x, y, idA, countA, idB, countB );

            totalCountA += countA;
            totalCountB += countB;
            }
        printf( "Average count %d (%s) = %f,  %d (%s) = %f  over %d runs\n",
                idA, getObject( idA )->description, 
                totalCountA / (double)numRuns,
                idB, getObject( idB )->description, 
                totalCountB / (double)numRuns,
                numRuns );
        printf( "Press ENTER to continue:\n" );
    
        int readInt;
        scanf( "%d", &readInt );
        }
    


    
    int port = 
        SettingsManager::getIntSetting( "port", 5077 );
    
    
    
    SocketServer *server = new SocketServer( port, 256 );
    
    sockPoll.addSocketServer( server );
    
    AppLog::infoF( "Listening for connection on port %d", port );

    // if we received one the last time we looped, don't sleep when
    // polling for socket being ready, because there could be more data
    // waiting in the buffer for a given socket
    char someClientMessageReceived = false;
    
    
    int shutdownMode = SettingsManager::getIntSetting( "shutdownMode", 0 );
    int forceShutdownMode = 
            SettingsManager::getIntSetting( "forceShutdownMode", 0 );
        
    
    // test code for printing sample eve locations
    // direct output from server to out.txt
    // then run:
    // grep "Eve location" out.txt | sed -e "s/Eve location //" | 
    //      sed -e "s/,/ /" > eveTest.txt
    // Then in gnuplot, do:
    //  plot "eveTest.txt" using 1:2 with linespoints;

    /*
    for( int i=0; i<1000; i++ ) {
        int x, y;
        
        SimpleVector<GridPos> temp;
        
        getEvePosition( "test@blah", 1, &x, &y, &temp, false );
        
        printf( "Eve location %d,%d\n", x, y );
        }
    */


    while( !quit ) {

        double curStepTime = Time::getCurrentTime();
        
        // flush past players hourly
        if( curStepTime - lastPastPlayerFlushTime > 3600 ) {
            
            // default one week
            int pastPlayerFlushTime = 
                SettingsManager::getIntSetting( "pastPlayerFlushTime", 604000 );
            
            for( int i=0; i<pastPlayers.size(); i++ ) {
                DeadObject *o = pastPlayers.getElement( i );
                
                if( curStepTime - o->lifeStartTimeSeconds > 
                    pastPlayerFlushTime ) {
                    // stale
                    delete [] o->name;
                    delete o->lineage;
                    pastPlayers.deleteElement( i );
                    i--;
                    } 
                }
            
            lastPastPlayerFlushTime = curStepTime;
            }
        
        
        char periodicStepThisStep = false;
        
        if( curStepTime - lastPeriodicStepTime > periodicStepTime ) {
            periodicStepThisStep = true;
            lastPeriodicStepTime = curStepTime;
            }
        
        
        if( periodicStepThisStep ) {
            shutdownMode = SettingsManager::getIntSetting( "shutdownMode", 0 );
            forceShutdownMode = 
                SettingsManager::getIntSetting( "forceShutdownMode", 0 );
            
            if( checkReadOnly() ) {
                // read-only file system causes all kinds of weird 
                // behavior
                // shut this server down NOW
                printf( "File system read only, forcing server shutdown.\n" );

                // force-run cron script one time here
                // this will send warning email to admin
                // (cron jobs stop running if filesystem read-only)
                system( "../scripts/checkServerRunningCron.sh" );

                shutdownMode = 1;
                forceShutdownMode = 1;
                }
            }
        
        
        if( forceShutdownMode ) {
            shutdownMode = 1;
        
            const char *shutdownMessage = "SD\n#";
            int messageLength = strlen( shutdownMessage );
            
            // send everyone who's still alive a shutdown message
            for( int i=0; i<players.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( i );
                
                if( nextPlayer->error ) {
                    continue;
                    }

                if( nextPlayer->connected ) {    
                    nextPlayer->sock->send( 
                        (unsigned char*)shutdownMessage, 
                        messageLength,
                        false, false );
                
                    nextPlayer->gotPartOfThisFrame = true;
                    }
                
                // don't worry about num sent
                // it's the last message to this client anyway
                setDeathReason( nextPlayer, 
                                "forced_shutdown" );
                nextPlayer->error = true;
                nextPlayer->errorCauseString =
                    "Forced server shutdown";
                }
            }
        else if( shutdownMode ) {
            // any disconnected players should be killed now
            for( int i=0; i<players.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( i );
                if( ! nextPlayer->error && ! nextPlayer->connected ) {
                    
                    setDeathReason( nextPlayer, 
                                    "disconnect_shutdown" );
                    
                    nextPlayer->error = true;
                    nextPlayer->errorCauseString =
                        "Disconnected during shutdown";
                    }
                }
            }
        

        if( periodicStepThisStep ) {
            
            apocalypseStep();
            monumentStep();
            
            updateSpecialBiomes( players.size() );
            
            //checkBackup();

            stepFoodLog();
            stepFailureLog();
            
            stepPlayerStats();
            stepLineageLog();
            stepCurseServerRequests();
            
            stepLifeTokens();
            stepFitnessScore();
            
            stepMapLongTermCulling( players.size() );
            
            stepArcReport();
            
            int arcMilestone = getArcYearsToReport( secondsPerYear, 100 );

            // don't send global arc messages if Eve injection on
            // arcs never end
            int eveInjectionOn = 
                SettingsManager::getIntSetting( "eveInjectionOn", 0 );
            
            if( arcMilestone != -1 && ! eveInjectionOn ) {

                int familyLimitAfterEveWindow = 
                    SettingsManager::getIntSetting( 
                        "familyLimitAfterEveWindow", 15 );
                
                int minFamiliesAfterEveWindow = 
                    SettingsManager::getIntSetting( 
                        "minFamiliesAfterEveWindow", 5 );

                char eveWindow = isEveWindow();

                char *familyLine;
                
                if( familyLimitAfterEveWindow > 0 &&
                    ! eveWindow ) {
                    familyLine = autoSprintf( "of %d",
                                              familyLimitAfterEveWindow );
                    }
                else {
                    familyLine = stringDuplicate( "" );
                    }

                const char *familyWord = "FAMILIES ARE";
                
                int numFams = countFamilies();
                
                if( numFams == 1 ) {
                    familyWord = "FAMILY IS";
                    }
                
                char *arcEndMessage;
                
                if( !eveWindow && minFamiliesAfterEveWindow > 0 ) {
                    arcEndMessage = autoSprintf( " (ARC ENDS BELOW %d)",
                                                 minFamiliesAfterEveWindow );
                    }
                else {
                    arcEndMessage = stringDuplicate( "" );
                    }
                

                char *message = autoSprintf( "ARC HAS LASTED %d YEARS.  "
                                             "ARC NAME IS :%s:**"
                                             "%d %s %s ALIVE%s",
                                             arcMilestone,
                                             getArcName(),
                                             numFams,
                                             familyLine,
                                             familyWord,
                                             arcEndMessage );
                delete [] familyLine;
                delete [] arcEndMessage;
                
                sendGlobalMessage( message );
                
                delete [] message;           
                }

            
            checkCustomGlobalMessage();
            }
        
        
        int numLive = players.size();



        if( shouldRunObjectSurvey() ) {
            SimpleVector<GridPos> livePlayerPos;
            
            for( int i=0; i<numLive; i++ ) {
                LiveObject *nextPlayer = players.getElement( i );
            
                if( nextPlayer->error ) {
                    continue;
                    }
                
                livePlayerPos.push_back( getPlayerPos( nextPlayer ) );
                }

            startObjectSurvey( &livePlayerPos );
            }
        
        stepObjectSurvey();
        
        stepLanguage();

        
        double secPerYear = 1.0 / getAgeRate();
        

        // check for timeout for shortest player move or food decrement
        // so that we wake up from listening to socket to handle it
        double minMoveTime = 999999;
        
        double curTime = Time::getCurrentTime();

        for( int i=0; i<numLive; i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            
            // clear at the start of each step
            nextPlayer->responsiblePlayerID = -1;

            if( nextPlayer->error ) {
                continue;
                }

            if( nextPlayer->xd != nextPlayer->xs ||
                nextPlayer->yd != nextPlayer->ys ) {
                
                double moveTimeLeft =
                    nextPlayer->moveTotalSeconds -
                    ( curTime - nextPlayer->moveStartTime );
                
                if( moveTimeLeft < 0 ) {
                    moveTimeLeft = 0;
                    }
                
                if( moveTimeLeft < minMoveTime ) {
                    minMoveTime = moveTimeLeft;
                    }
                }
            

            double timeLeft = minMoveTime;
            
            if( ! nextPlayer->vogMode ) {
                // look at food decrement time too
                
                timeLeft =
                    nextPlayer->foodDecrementETASeconds - curTime;
                
                if( timeLeft < 0 ) {
                    timeLeft = 0;
                    }
                if( timeLeft < minMoveTime ) {
                    minMoveTime = timeLeft;
                    }           
                }
            
            // look at held decay too
            if( nextPlayer->holdingEtaDecay != 0 ) {
                
                timeLeft = nextPlayer->holdingEtaDecay - curTime;
                
                if( timeLeft < 0 ) {
                    timeLeft = 0;
                    }
                if( timeLeft < minMoveTime ) {
                    minMoveTime = timeLeft;
                    }
                }
            
            for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
                if( nextPlayer->clothingEtaDecay[c] != 0 ) {
                    timeLeft = nextPlayer->clothingEtaDecay[c] - curTime;
                    
                    if( timeLeft < 0 ) {
                        timeLeft = 0;
                        }
                    if( timeLeft < minMoveTime ) {
                        minMoveTime = timeLeft;
                        }
                    }
                for( int cc=0; cc<nextPlayer->clothingContained[c].size();
                     cc++ ) {
                    timeSec_t decay =
                        nextPlayer->clothingContainedEtaDecays[c].
                        getElementDirect( cc );
                    
                    if( decay != 0 ) {
                        timeLeft = decay - curTime;
                        
                        if( timeLeft < 0 ) {
                            timeLeft = 0;
                            }
                        if( timeLeft < minMoveTime ) {
                            minMoveTime = timeLeft;
                            }
                        }
                    }
                }
            
            // look at old age death to
            double ageLeft = forceDeathAge - computeAge( nextPlayer );
            
            double ageSecondsLeft = ageLeft * secPerYear;
            
            if( ageSecondsLeft < minMoveTime ) {
                minMoveTime = ageSecondsLeft;

                if( minMoveTime < 0 ) {
                    minMoveTime = 0;
                    }
                }
            

            // as low as it can get, no need to check other players
            if( minMoveTime == 0 ) {
                break;
                }
            }
        
        
        SocketOrServer *readySock =  NULL;

        double pollTimeout = 2;
        
        if( minMoveTime < pollTimeout ) {
            // shorter timeout if we have to wake up for a move
            
            // HOWEVER, always keep max timout at 2 sec
            // so we always wake up periodically to catch quit signals, etc

            pollTimeout = minMoveTime;
            }
        
        if( pollTimeout > 0 ) {
            int shortestDecay = getNextDecayDelta();
            
            if( shortestDecay != -1 ) {
                
                if( shortestDecay < pollTimeout ) {
                    pollTimeout = shortestDecay;
                    }
                }
            }

        
        char anyTicketServerRequestsOut = false;

        for( int i=0; i<newConnections.size(); i++ ) {
            
            FreshConnection *nextConnection = newConnections.getElement( i );

            if( nextConnection->ticketServerRequest != NULL ) {
                anyTicketServerRequestsOut = true;
                break;
                }
            }
        
        if( anyTicketServerRequestsOut ) {
            // need to step outstanding ticket server web requests
            // sleep a tiny amount of time to avoid cpu spin
            pollTimeout = 0.01;
            }


        if( areTriggersEnabled() ) {
            // need to handle trigger timing
            pollTimeout = 0.01;
            }

        if( someClientMessageReceived ) {
            // don't wait at all
            // we need to check for next message right away
            pollTimeout = 0;
            }

        if( tutorialLoadingPlayers.size() > 0 ) {
            // don't wait at all if there are tutorial maps to load
            pollTimeout = 0;
            }
        

        if( pollTimeout > 0.1 && activeKillStates.size() > 0 ) {
            // we have active kill requests pending
            // want a short timeout so that we can catch kills 
            // when player's paths cross
            pollTimeout = 0.1;
            }
        

        // we thus use zero CPU as long as no messages or new connections
        // come in, and only wake up when some timed action needs to be
        // handled
        
        readySock = sockPoll.wait( (int)( pollTimeout * 1000 ) );
        
        
        
        
        if( readySock != NULL && !readySock->isSocket ) {
            // server ready
            Socket *sock = server->acceptConnection( 0 );

            if( sock != NULL ) {
                HostAddress *a = sock->getRemoteHostAddress();
                
                if( a == NULL ) {    
                    AppLog::info( "Got connection from unknown address" );
                    }
                else {
                    AppLog::infoF( "Got connection from %s:%d",
                                  a->mAddressString, a->mPort );
                    delete a;
                    }
            

                FreshConnection newConnection;
                
                newConnection.connectionStartTimeSeconds = 
                    Time::getCurrentTime();

                newConnection.email = NULL;

                newConnection.sock = sock;

                newConnection.sequenceNumber = nextSequenceNumber;

                

                char *secretString = 
                    SettingsManager::getStringSetting( 
                        "statsServerSharedSecret", "sdfmlk3490sadfm3ug9324" );

                char *numberString = 
                    autoSprintf( "%lu", newConnection.sequenceNumber );
                
                char *nonce = hmac_sha1( secretString, numberString );

                delete [] secretString;
                delete [] numberString;

                newConnection.sequenceNumberString = 
                    autoSprintf( "%s%lu", nonce, 
                                 newConnection.sequenceNumber );
                
                delete [] nonce;
                    

                newConnection.tutorialNumber = 0;
                newConnection.curseStatus.curseLevel = 0;
                newConnection.curseStatus.excessPoints = 0;

                newConnection.twinCode = NULL;
                newConnection.twinCount = 0;
                
                newConnection.clientTag = NULL;
                
                nextSequenceNumber ++;
                
                SettingsManager::setSetting( "sequenceNumber",
                                             (int)nextSequenceNumber );
                
                char *message;
                
                int maxPlayers = 
                    SettingsManager::getIntSetting( "maxPlayers", 200 );
                
                int currentPlayers = players.size() + newConnections.size();
                    

                if( apocalypseTriggered || shutdownMode ) {
                        
                    AppLog::info( "We are in shutdown mode, "
                                  "deflecting new connection" );         
                    
                    AppLog::infoF( "%d player(s) still alive on server.",
                                   players.size() );

                    message = autoSprintf( "SHUTDOWN\n"
                                           "%d/%d\n"
                                           "#",
                                           currentPlayers, maxPlayers );
                    newConnection.shutdownMode = true;
                    }
                else if( currentPlayers >= maxPlayers ) {
                    AppLog::infoF( "%d of %d permitted players connected, "
                                   "deflecting new connection",
                                   currentPlayers, maxPlayers );
                    
                    message = autoSprintf( "SERVER_FULL\n"
                                           "%d/%d\n"
                                           "#",
                                           currentPlayers, maxPlayers );
                    
                    newConnection.shutdownMode = true;
                    }         
                else {
                    message = autoSprintf( "SN\n"
                                           "%d/%d\n"
                                           "%s\n"
                                           "%lu\n#",
                                           currentPlayers, maxPlayers,
                                           newConnection.sequenceNumberString,
                                           versionNumber );
                    newConnection.shutdownMode = false;
                    }


                // wait for email and hashes to come from client
                // (and maybe ticket server check isn't required by settings)
                newConnection.ticketServerRequest = NULL;
                newConnection.ticketServerAccepted = false;
                newConnection.lifeTokenSpent = false;
                
                // -1 is a possible score now
                // use -99999 as still-waiting marker
                newConnection.fitnessScore = -99999;

                newConnection.error = false;
                newConnection.errorCauseString = "";
                newConnection.rejectedSendTime = 0;
                
                int messageLength = strlen( message );
                
                int numSent = 
                    sock->send( (unsigned char*)message, 
                                messageLength, 
                                false, false );
                    
                delete [] message;
                    

                if( numSent != messageLength ) {
                    // failed or blocked on our first send attempt

                    // reject it right away

                    delete sock;
                    sock = NULL;
                    }
                else {
                    // first message sent okay
                    newConnection.sockBuffer = new SimpleVector<char>();
                    

                    sockPoll.addSocket( sock );

                    newConnections.push_back( newConnection );
                    }

                AppLog::infoF( "Listening for another connection on port %d", 
                               port );
    
                }
            }
        

        stepTriggers();
        
        
        // listen for messages from new connections
        double currentTime = Time::getCurrentTime();
        
        for( int i=0; i<newConnections.size(); i++ ) {
            
            FreshConnection *nextConnection = newConnections.getElement( i );
            
            if( nextConnection->error ) {
                continue;
                }
            
            if( nextConnection->email != NULL &&
                nextConnection->curseStatus.curseLevel == -1 ) {
                // keep checking if curse level has arrived from
                // curse server
                nextConnection->curseStatus =
                    getCurseLevel( nextConnection->email );
                if( nextConnection->curseStatus.curseLevel != -1 ) {
                    AppLog::infoF( 
                        "Got curse level for %s from curse server: "
                        "%d (excess %d)",
                        nextConnection->email,
                        nextConnection->curseStatus.curseLevel,
                        nextConnection->curseStatus.excessPoints );
                    }
                }
            else if( nextConnection->email != NULL &&
                nextConnection->lifeStats.lifeCount == -1 ) {
                // keep checking if life stats have arrived from
                // stats server
                int statsResult = getPlayerLifeStats( nextConnection->email,
                    &( nextConnection->lifeStats.lifeCount ),
                    &( nextConnection->lifeStats.lifeTotalSeconds ) );
                
                if( statsResult == -1 ) {
                    // error
                    // it's done now!
                    nextConnection->lifeStats.lifeCount = 0;
                    nextConnection->lifeStats.lifeTotalSeconds = 0;
                    nextConnection->lifeStats.error = true;
                    }
                else if( statsResult == 1 ) {
                    AppLog::infoF( 
                        "Got life stats for %s from stats server: "
                        "%d lives, %d total seconds (%.2lf hours)",
                        nextConnection->email,
                        nextConnection->lifeStats.lifeCount,
                        nextConnection->lifeStats.lifeTotalSeconds,
                        nextConnection->lifeStats.lifeTotalSeconds / 3600.0 );
                    }
                }
            else if( nextConnection->email != NULL &&
                     nextConnection->fitnessScore == -99999 ) {
                // still waiting for fitness score
                int fitResult = 
                    getFitnessScore( nextConnection->email, 
                                     &nextConnection->fitnessScore );
                
                if( fitResult == -1 ) {
                    // failed
                    // stop asking now
                    nextConnection->fitnessScore = 0;
                    }
                }
            else if( nextConnection->ticketServerRequest != NULL &&
                     ! nextConnection->ticketServerAccepted ) {
                
                int result;

                if( currentTime - nextConnection->ticketServerRequestStartTime
                    < 8 ) {
                    // 8-second timeout on ticket server requests
                    result = nextConnection->ticketServerRequest->step();
                    }
                else {
                    result = -1;
                    }

                if( result == -1 ) {
                    AppLog::info( "Request to ticket server failed, "
                                  "client rejected." );
                    nextConnection->error = true;
                    nextConnection->errorCauseString =
                        "Ticket server failed";
                    }
                else if( result == 1 ) {
                    // done, have result

                    char *webResult = 
                        nextConnection->ticketServerRequest->getResult();
                    
                    if( strstr( webResult, "INVALID" ) != NULL ) {
                        AppLog::info( 
                            "Client key hmac rejected by ticket server, "
                            "client rejected." );
                        nextConnection->error = true;
                        nextConnection->errorCauseString =
                            "Client key check failed";
                        }
                    else if( strstr( webResult, "VALID" ) != NULL ) {
                        // correct!
                        nextConnection->ticketServerAccepted = true;
                        }
                    else {
                        AppLog::errorF( 
                            "Unexpected result from ticket server, "
                            "client rejected:  %s", webResult );
                        nextConnection->error = true;
                        nextConnection->errorCauseString =
                            "Client key check failed "
                            "(bad ticketServer response)";
                        }
                    delete [] webResult;
                    }
                }
            else if( nextConnection->ticketServerRequest != NULL &&
                     nextConnection->ticketServerAccepted &&
                     ! nextConnection->lifeTokenSpent ) {

                char liveButDisconnected = false;
                
                for( int p=0; p<players.size(); p++ ) {
                    LiveObject *o = players.getElement( p );
                    if( ! o->error && 
                        ! o->connected && 
                        strcmp( o->email, 
                                nextConnection->email ) == 0 ) {
                        liveButDisconnected = true;
                        break;
                        }
                    }

                if( liveButDisconnected ) {
                    // spent when they first connected, don't respend now
                    nextConnection->lifeTokenSpent = true;
                    }
                else {
                    int spendResult = 
                        spendLifeToken( nextConnection->email );
                    if( spendResult == -1 ) {
                        AppLog::info( 
                            "Failed to spend life token for client, "
                            "client rejected." );

                        const char *message = "NO_LIFE_TOKENS\n#";
                        nextConnection->sock->send( (unsigned char*)message,
                                                    strlen( message ), 
                                                    false, false );

                        nextConnection->error = true;
                        nextConnection->errorCauseString =
                            "Client life token spend failed";
                        }
                    else if( spendResult == 1 ) {
                        nextConnection->lifeTokenSpent = true;
                        }
                    }
                }
            else if( nextConnection->ticketServerRequest != NULL &&
                     nextConnection->ticketServerAccepted &&
                     nextConnection->lifeTokenSpent ) {
                // token spent successfully (or token server not used)

                const char *message = "ACCEPTED\n#";
                int messageLength = strlen( message );
                
                int numSent = 
                    nextConnection->sock->send( 
                        (unsigned char*)message, 
                        messageLength, 
                        false, false );
                        

                if( numSent != messageLength ) {
                    AppLog::info( "Failed to write to client socket, "
                                  "client rejected." );
                    nextConnection->error = true;
                    nextConnection->errorCauseString =
                        "Socket write failed";

                    }
                else {
                    // ready to start normal message exchange
                    // with client
                            
                    AppLog::info( "Got new player logged in" );
                    logClientTag( nextConnection );

                    delete nextConnection->ticketServerRequest;
                    nextConnection->ticketServerRequest = NULL;

                    delete [] nextConnection->sequenceNumberString;
                    nextConnection->sequenceNumberString = NULL;
                    
                    delete [] nextConnection->clientTag;
                    nextConnection->clientTag = NULL;

                    if( nextConnection->twinCode != NULL
                        && 
                        nextConnection->twinCount > 0 ) {
                        processWaitingTwinConnection( *nextConnection );
                        }
                    else {
                        if( nextConnection->twinCode != NULL ) {
                            delete [] nextConnection->twinCode;
                            nextConnection->twinCode = NULL;
                            }
                                
                        processLoggedInPlayer( 
                            true,
                            nextConnection->sock,
                            nextConnection->sockBuffer,
                            nextConnection->email,
                            nextConnection->tutorialNumber,
                            nextConnection->curseStatus,
                            nextConnection->lifeStats,
                            nextConnection->fitnessScore );
                        }
                                                        
                    newConnections.deleteElement( i );
                    i--;
                    }
                }
            else if( nextConnection->ticketServerRequest == NULL ) {

                double timeDelta = Time::getCurrentTime() -
                    nextConnection->connectionStartTimeSeconds;
                

                

                char result = 
                    readSocketFull( nextConnection->sock,
                                    nextConnection->sockBuffer );
                
                if( ! result ) {
                    AppLog::info( "Failed to read from client socket, "
                                  "client rejected." );
                    nextConnection->error = true;
                    
                    // force connection close right away
                    // don't send REJECTED message and wait
                    nextConnection->rejectedSendTime = 1;
                    
                    nextConnection->errorCauseString =
                        "Socket read failed";
                    }
                
                char *message = NULL;
                int timeLimit = 10;
                
                if( ! nextConnection->shutdownMode ) {
                    message = 
                        getNextClientMessage( nextConnection->sockBuffer );
                    }
                else {
                    timeLimit = 5;
                    }
                
                if( message != NULL ) {
                    
                    
                    if( strstr( message, "LOGIN" ) != NULL ) {
                        
                        SimpleVector<char *> *tokens =
                            tokenizeString( message );
                        
                        if( strstr( message, "client_" ) != NULL ) {
                            // new client_ parameter
                            
                            // it is the first parameter after LOGIN
                            
                            // save and remove it
                            nextConnection->clientTag = 
                                tokens->getElementDirect( 1 );
                            
                            tokens->deleteElement( 1 );
                            }

                        if( tokens->size() == 4 || tokens->size() == 5 ||
                            tokens->size() == 7 ) {
                            
                            nextConnection->email = 
                                stringDuplicate( 
                                    tokens->getElementDirect( 1 ) );
                            char *pwHash = tokens->getElementDirect( 2 );
                            char *keyHash = tokens->getElementDirect( 3 );
                            
                            if( tokens->size() >= 5 ) {
                                sscanf( tokens->getElementDirect( 4 ),
                                        "%d", 
                                        &( nextConnection->tutorialNumber ) );
                                }
                            
                            if( tokens->size() == 7 ) {
                                nextConnection->twinCode =
                                    stringDuplicate( 
                                        tokens->getElementDirect( 5 ) );
                                
                                sscanf( tokens->getElementDirect( 6 ),
                                        "%d", 
                                        &( nextConnection->twinCount ) );

                                int maxCount = 
                                    SettingsManager::getIntSetting( 
                                        "maxTwinPartySize", 4 );
                                
                                if( nextConnection->twinCount > maxCount ) {
                                    nextConnection->twinCount = maxCount;
                                    }
                                }
                            

                            // this may return -1 if curse server
                            // request is pending
                            // we'll catch that case later above
                            nextConnection->curseStatus =
                                getCurseLevel( nextConnection->email );


                            nextConnection->lifeStats.lifeCount = -1;
                            nextConnection->lifeStats.lifeTotalSeconds = -1;
                            nextConnection->lifeStats.error = false;
                            
                            // this will leave them as -1 if request pending
                            // we'll catch that case later above
                            int statsResult = getPlayerLifeStats(
                                nextConnection->email,
                                &( nextConnection->
                                   lifeStats.lifeCount ),
                                &( nextConnection->
                                   lifeStats.lifeTotalSeconds ) );

                            if( statsResult == -1 ) {
                                // error
                                // it's done now!
                                nextConnection->lifeStats.lifeCount = 0;
                                nextConnection->lifeStats.lifeTotalSeconds = 0;
                                nextConnection->lifeStats.error = true;
                                }
                                


                            if( requireClientPassword &&
                                ! nextConnection->error  ) {

                                char *trueHash = 
                                    hmac_sha1( 
                                        clientPassword, 
                                        nextConnection->sequenceNumberString );
                                

                                if( strcmp( trueHash, pwHash ) != 0 ) {
                                    AppLog::info( "Client password hmac bad, "
                                                  "client rejected." );
                                    nextConnection->error = true;
                                    nextConnection->errorCauseString =
                                        "Password check failed";
                                    }

                                delete [] trueHash;
                                }
                            
                            if( requireTicketServerCheck &&
                                ! nextConnection->error ) {
                                
                                char *encodedEmail =
                                    URLUtils::urlEncode( 
                                        nextConnection->email );

                                char *url = autoSprintf( 
                                    "%s?action=check_ticket_hash"
                                    "&email=%s"
                                    "&hash_value=%s"
                                    "&string_to_hash=%s",
                                    ticketServerURL,
                                    encodedEmail,
                                    keyHash,
                                    nextConnection->sequenceNumberString );

                                delete [] encodedEmail;

                                nextConnection->ticketServerRequest =
                                    new WebRequest( "GET", url, NULL );
                                nextConnection->ticketServerAccepted = false;

                                nextConnection->ticketServerRequestStartTime
                                    = currentTime;

                                delete [] url;
                                }
                            else if( !requireTicketServerCheck &&
                                     !nextConnection->error ) {
                                
                                // let them in without checking
                                
                                const char *message = "ACCEPTED\n#";
                                int messageLength = strlen( message );
                
                                int numSent = 
                                    nextConnection->sock->send( 
                                        (unsigned char*)message, 
                                        messageLength, 
                                        false, false );
                        

                                if( numSent != messageLength ) {
                                    AppLog::info( 
                                        "Failed to send on client socket, "
                                        "client rejected." );
                                    nextConnection->error = true;
                                    nextConnection->errorCauseString =
                                        "Socket write failed";
                                    }
                                else {
                                    // ready to start normal message exchange
                                    // with client
                            
                                    AppLog::info( "Got new player logged in" );
                                    logClientTag( nextConnection );
                                    
                                    delete nextConnection->ticketServerRequest;
                                    nextConnection->ticketServerRequest = NULL;
                                    
                                    delete [] 
                                        nextConnection->sequenceNumberString;
                                    nextConnection->sequenceNumberString = NULL;

                                    delete [] nextConnection->clientTag;
                                    nextConnection->clientTag = NULL;


                                    if( nextConnection->twinCode != NULL
                                        && 
                                        nextConnection->twinCount > 0 ) {
                                        processWaitingTwinConnection(
                                            *nextConnection );
                                        }
                                    else {
                                        if( nextConnection->twinCode != NULL ) {
                                            delete [] nextConnection->twinCode;
                                            nextConnection->twinCode = NULL;
                                            }
                                        processLoggedInPlayer(
                                            true,
                                            nextConnection->sock,
                                            nextConnection->sockBuffer,
                                            nextConnection->email,
                                            nextConnection->tutorialNumber,
                                            nextConnection->curseStatus,
                                            nextConnection->lifeStats,
                                            nextConnection->fitnessScore );
                                        }
                                                                        
                                    newConnections.deleteElement( i );
                                    i--;
                                    }
                                }
                            }
                        else {
                            AppLog::info( "LOGIN message has wrong format, "
                                          "client rejected." );
                            nextConnection->error = true;
                            nextConnection->errorCauseString =
                                "Bad login message";
                            }


                        tokens->deallocateStringElements();
                        delete tokens;
                        }
                    else {
                        AppLog::info( "Client's first message not LOGIN, "
                                      "client rejected." );
                        nextConnection->error = true;
                        nextConnection->errorCauseString =
                            "Unexpected first message";
                        }
                    
                    delete [] message;
                    }
                else if( timeDelta > timeLimit ) {
                    if( nextConnection->shutdownMode ) {
                        AppLog::info( "5 second grace period for new "
                                      "connection in shutdown mode, closing." );
                        }
                    else {
                        AppLog::info( 
                            "Client failed to LOGIN after 10 seconds, "
                            "client rejected." );
                        }
                    nextConnection->error = true;
                    nextConnection->errorCauseString =
                        "Login timeout";
                    }
                }
            }
            


        // make sure all twin-waiting sockets are still connected
        for( int i=0; i<waitingForTwinConnections.size(); i++ ) {
            FreshConnection *nextConnection = 
                waitingForTwinConnections.getElement( i );
            
            char result = 
                readSocketFull( nextConnection->sock,
                                nextConnection->sockBuffer );
            
            if( ! result ) {
                AppLog::info( "Failed to read from twin-waiting client socket, "
                              "client rejected." );

                refundLifeToken( nextConnection->email );
                
                nextConnection->error = true;

                // force connection close right away
                // don't send REJECTED message and wait
                nextConnection->rejectedSendTime = 1;
                    
                nextConnection->errorCauseString =
                    "Socket read failed";
                }
            }
            
        

        // now clean up any new connections that have errors
        
        // FreshConnections are in two different lists
        // clean up errors in both
        currentTime = Time::getCurrentTime();
        
        SimpleVector<FreshConnection> *connectionLists[2] =
            { &newConnections, &waitingForTwinConnections };
        for( int c=0; c<2; c++ ) {
            SimpleVector<FreshConnection> *list = connectionLists[c];
        
            for( int i=0; i<list->size(); i++ ) {
            
                FreshConnection *nextConnection = list->getElement( i );
            
                if( nextConnection->error ) {
                
                    if( nextConnection->rejectedSendTime == 0 ) {
                        
                        // try sending REJECTED message at end
                        // give them 5 seconds to receive it before closing
                        // the connection
                        const char *message = "REJECTED\n#";
                        nextConnection->sock->send( (unsigned char*)message,
                                                    strlen( message ), 
                                                false, false );
                        nextConnection->rejectedSendTime = currentTime;
                        }
                    else if( currentTime - nextConnection->rejectedSendTime >
                             5 ) {
                        // 5 sec passed since REJECTED sent
                        
                        AppLog::infoF( "Closing new connection on error "
                                       "(cause: %s)",
                                       nextConnection->errorCauseString );
                        
                        if( nextConnection->sock != NULL ) {
                            sockPoll.removeSocket( nextConnection->sock );
                            }
                        
                        deleteMembers( nextConnection );
                        
                        list->deleteElement( i );
                        i--;
                        }
                    }
                }
            }
    

        // step tutorial map load for player at front of line
        
        // 5 ms
        double timeLimit = 0.005;
        
        for( int i=0; i<tutorialLoadingPlayers.size(); i++ ) {
            LiveObject *nextPlayer = tutorialLoadingPlayers.getElement( i );
            
            char moreLeft = loadTutorialStep( &( nextPlayer->tutorialLoad ),
                                              timeLimit );
            
            if( moreLeft ) {
                // only load one step from first in line
                break;
                }
            
            // first in line is done
            
            AppLog::infoF( "New player %s tutorial loaded after %u steps, "
                           "%f total sec (loadID = %u )",
                           nextPlayer->email,
                           nextPlayer->tutorialLoad.stepCount,
                           Time::getCurrentTime() - 
                           nextPlayer->tutorialLoad.startTime,
                           nextPlayer->tutorialLoad.uniqueLoadID );

            // remove it and any twins
            unsigned int uniqueID = nextPlayer->tutorialLoad.uniqueLoadID;
            

            players.push_back( *nextPlayer );

            tutorialLoadingPlayers.deleteElement( i );
            
            LiveObject *twinPlayer = NULL;
            
            if( i < tutorialLoadingPlayers.size() ) {
                twinPlayer = tutorialLoadingPlayers.getElement( i );
                }
            
            while( twinPlayer != NULL && 
                   twinPlayer->tutorialLoad.uniqueLoadID == uniqueID ) {
                
                AppLog::infoF( "Twin %s tutorial loaded too (loadID = %u )",
                               twinPlayer->email,
                               uniqueID );
            
                players.push_back( *twinPlayer );

                tutorialLoadingPlayers.deleteElement( i );
                
                twinPlayer = NULL;
                
                if( i < tutorialLoadingPlayers.size() ) {
                    twinPlayer = tutorialLoadingPlayers.getElement( i );
                    }
                }
            break;
            
            }
        


        
    
        someClientMessageReceived = false;

        numLive = players.size();
        

        // listen for any messages from clients 

        // track index of each player that needs an update sent about it
        // we compose the full update message below
        SimpleVector<int> playerIndicesToSendUpdatesAbout;
        
        SimpleVector<int> playerIndicesToSendLineageAbout;

        SimpleVector<int> playerIndicesToSendCursesAbout;

        SimpleVector<int> playerIndicesToSendNamesAbout;

        SimpleVector<int> playerIndicesToSendDyingAbout;

        SimpleVector<int> playerIndicesToSendHealingAbout;


        SimpleVector<GridPos> newOwnerPos;

        newOwnerPos.push_back_other( &recentlyRemovedOwnerPos );
        recentlyRemovedOwnerPos.deleteAll();


        SimpleVector<UpdateRecord> newUpdates;
        SimpleVector<ChangePosition> newUpdatesPos;
        SimpleVector<int> newUpdatePlayerIDs;


        // these are global, so they're not tagged with positions for
        // spatial filtering
        SimpleVector<UpdateRecord> newDeleteUpdates;
        

        SimpleVector<MapChangeRecord> mapChanges;
        SimpleVector<ChangePosition> mapChangesPos;
        

        SimpleVector<FlightDest> newFlightDest;
        


        
        timeSec_t curLookTime = Time::timeSec();
        
        for( int i=0; i<numLive; i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            
            nextPlayer->updateSent = false;

            if( nextPlayer->error ) {
                continue;
                }
            
            if( nextPlayer->numToolSlots == -1 ) {
                
                setupToolSlots( nextPlayer );
                }


            double curCrossTime = Time::getCurrentTime();

            char checkCrossing = true;
            
            if( curCrossTime < nextPlayer->playerCrossingCheckTime +
                playerCrossingCheckStepTime ) {
                // player not due for another check yet
                checkCrossing = false;
                }
            else {
                // time for next check for this player
                nextPlayer->playerCrossingCheckTime = curCrossTime;
                checkCrossing = true;
                }
            
            
            if( checkCrossing ) {
                GridPos curPos = { nextPlayer->xd, nextPlayer->yd };
            
                if( nextPlayer->xd != nextPlayer->xs ||
                    nextPlayer->yd != nextPlayer->ys ) {
                
                    curPos = computePartialMoveSpot( nextPlayer );
                    }
            
                int curOverID = getMapObject( curPos.x, curPos.y );
            

                if( ! nextPlayer->heldByOther &&
                    ! nextPlayer->vogMode &&
                    curOverID != 0 && 
                    ! isMapObjectInTransit( curPos.x, curPos.y ) &&
                    ! wasRecentlyDeadly( curPos ) ) {
                
                    ObjectRecord *curOverObj = getObject( curOverID );
                
                    char riding = false;
                
                    if( nextPlayer->holdingID > 0 && 
                        getObject( nextPlayer->holdingID )->rideable ) {
                        riding = true;
                        }

                    if( !riding &&
                        curOverObj->permanent && 
                        curOverObj->deadlyDistance > 0 ) {
                    
                        char wasSick = false;
                                        
                        if( nextPlayer->holdingID > 0 &&
                            strstr(
                                getObject( nextPlayer->holdingID )->
                                description,
                                "sick" ) != NULL ) {
                            wasSick = true;
                            }


                        addDeadlyMapSpot( curPos );
                    
                        setDeathReason( nextPlayer, 
                                        "killed",
                                        curOverID );
                    
                        nextPlayer->deathSourceID = curOverID;
                    
                        if( curOverObj->isUseDummy ) {
                            nextPlayer->deathSourceID = 
                                curOverObj->useDummyParent;
                            }

                        nextPlayer->errorCauseString =
                            "Player killed by permanent object";
                    
                        if( ! nextPlayer->dying || wasSick ) {
                            // if was sick, they had a long stagger
                            // time set, so cutting it in half makes no sense
                        
                            int staggerTime = 
                                SettingsManager::getIntSetting(
                                    "deathStaggerTime", 20 );
                        
                            double currentTime = 
                                Time::getCurrentTime();
                        
                            nextPlayer->dying = true;
                            nextPlayer->dyingETA = 
                                currentTime + staggerTime;

                            playerIndicesToSendDyingAbout.
                                push_back( 
                                    getLiveObjectIndex( 
                                        nextPlayer->id ) );
                            }
                        else {
                            // already dying, and getting attacked again
                        
                            // halve their remaining stagger time
                            double currentTime = 
                                Time::getCurrentTime();
                        
                            double staggerTimeLeft = 
                                nextPlayer->dyingETA - currentTime;
                        
                            if( staggerTimeLeft > 0 ) {
                                staggerTimeLeft /= 2;
                                nextPlayer->dyingETA = 
                                    currentTime + staggerTimeLeft;
                                }
                            }
                
                    
                        // generic on-person
                        TransRecord *r = 
                            getPTrans( curOverID, 0 );

                        if( r != NULL ) {
                            setMapObject( curPos.x, curPos.y, r->newActor );

                            // new target specifies wound
                            // but never replace an existing wound
                            // death time is shortened above
                            // however, wounds can replace sickness 
                            if( r->newTarget > 0 &&
                                ( ! nextPlayer->holdingWound || wasSick ) ) {
                                // don't drop their wound
                                if( nextPlayer->holdingID != 0 &&
                                    ! nextPlayer->holdingWound &&
                                    ! nextPlayer->holdingBiomeSickness ) {
                                    handleDrop( 
                                        curPos.x, curPos.y, 
                                        nextPlayer,
                                        &playerIndicesToSendUpdatesAbout );
                                    }
                                nextPlayer->holdingID = 
                                    r->newTarget;
                                holdingSomethingNew( nextPlayer );
                            
                                setFreshEtaDecayForHeld( nextPlayer );
                            
                                checkSickStaggerTime( nextPlayer );
                                
                                
                                nextPlayer->holdingWound = true;
                                nextPlayer->holdingBiomeSickness = false;
                                
                                ForcedEffects e = 
                                    checkForForcedEffects( 
                                        nextPlayer->holdingID );
                            
                                if( e.emotIndex != -1 ) {
                                    nextPlayer->emotFrozen = true;
                                    nextPlayer->emotFrozenIndex = e.emotIndex;
                                    
                                    newEmotPlayerIDs.push_back( 
                                        nextPlayer->id );
                                    newEmotIndices.push_back( e.emotIndex );
                                    newEmotTTLs.push_back( e.ttlSec );
                                    interruptAnyKillEmots( nextPlayer->id,
                                                           e.ttlSec );
                                    }
                                if( e.foodModifierSet && 
                                    e.foodCapModifier != 1 ) {
                                    nextPlayer->yummyBonusStore = 0;
                                    nextPlayer->foodCapModifier = 
                                        e.foodCapModifier;
                                    nextPlayer->foodUpdate = true;
                                    }
                                if( e.feverSet ) {
                                    nextPlayer->fever = e.fever;
                                    }
                            

                                playerIndicesToSendUpdatesAbout.
                                    push_back( 
                                        getLiveObjectIndex( 
                                            nextPlayer->id ) );
                                }
                            }
                        }
                    else if( riding && 
                             curOverObj->permanent && 
                             curOverObj->deadlyDistance > 0 ) {
                        // rode over something deadly
                        // see if it affects what we're riding

                        TransRecord *r = 
                            getPTrans( nextPlayer->holdingID, curOverID );
                    
                        if( r != NULL ) {
                            handleHoldingChange( nextPlayer,
                                                 r->newActor );
                            nextPlayer->heldTransitionSourceID = curOverID;
                            playerIndicesToSendUpdatesAbout.push_back( i );

                            setMapObject( curPos.x, curPos.y, r->newTarget );

                            // it attacked their vehicle 
                            // put it on cooldown so it won't immediately
                            // attack them
                            addDeadlyMapSpot( curPos );
                            }
                        }                
                    }
                }
            
            
            if( curLookTime - nextPlayer->lastRegionLookTime > 5 ) {
                lookAtRegion( nextPlayer->xd - 8, nextPlayer->yd - 7,
                              nextPlayer->xd + 8, nextPlayer->yd + 7 );
                nextPlayer->lastRegionLookTime = curLookTime;
                }

            char *message = NULL;
            
            if( nextPlayer->connected ) {    
                char result = 
                    readSocketFull( nextPlayer->sock, nextPlayer->sockBuffer );
            
                if( ! result ) {
                    setPlayerDisconnected( nextPlayer, "Socket read failed" );
                    }
                else {
                    // don't even bother parsing message buffer for players
                    // that are not currently connected
                    message = getNextClientMessage( nextPlayer->sockBuffer );
                    }
                }
            
            
            if( message != NULL ) {
                someClientMessageReceived = true;
                
                AppLog::infoF( "Got client message from %d: %s",
                               nextPlayer->id, message );
                
                ClientMessage m = parseMessage( nextPlayer, message );
                
                delete [] message;
                
                if( m.type == UNKNOWN ) {
                    AppLog::info( "Client error, unknown message type." );
                    
                    setPlayerDisconnected( nextPlayer, 
                                           "Unknown message type" );
                    }

                //Thread::staticSleep( 
                //    testRandSource.getRandomBoundedInt( 0, 450 ) );
                
                if( m.type == BUG ) {
                    int allow = 
                        SettingsManager::getIntSetting( "allowBugReports", 0 );

                    if( allow ) {
                        char *bugName = 
                            autoSprintf( "bug_%d_%d_%f",
                                         m.bug,
                                         nextPlayer->id,
                                         Time::getCurrentTime() );
                        char *bugInfoName = autoSprintf( "%s_info.txt",
                                                         bugName );
                        char *bugOutName = autoSprintf( "%s_out.txt",
                                                        bugName );
                        FILE *bugInfo = fopen( bugInfoName, "w" );
                        if( bugInfo != NULL ) {
                            fprintf( bugInfo, 
                                     "Bug report from player %d\n"
                                     "Bug text:  %s\n", 
                                     nextPlayer->id,
                                     m.bugText );
                            fclose( bugInfo );
                            
                            File outFile( NULL, "serverOut.txt" );
                            if( outFile.exists() ) {
                                fflush( stdout );
                                File outCopyFile( NULL, bugOutName );
                                
                                outFile.copy( &outCopyFile );
                                }
                            }
                        delete [] bugName;
                        delete [] bugInfoName;
                        delete [] bugOutName;
                        }
                    }
                else if( m.type == MAP ) {
                    
                    int allow = 
                        SettingsManager::getIntSetting( "allowMapRequests", 0 );
                    

                    if( allow ) {
                        
                        SimpleVector<char *> *list = 
                            SettingsManager::getSetting( 
                                "mapRequestAllowAccounts" );
                        
                        allow = false;
                        
                        for( int i=0; i<list->size(); i++ ) {
                            if( strcmp( nextPlayer->email,
                                        list->getElementDirect( i ) ) == 0 ) {
                                
                                allow = true;
                                break;
                                }
                            }
                        
                        list->deallocateStringElements();
                        delete list;
                        }
                    

                    if( allow && nextPlayer->connected ) {
                        
                        // keep them full of food so they don't 
                        // die of hunger during the pull
                        nextPlayer->foodStore = 
                            computeFoodCapacity( nextPlayer );
                        

                        int length;

                        // map chunks sent back to client absolute
                        // relative to center instead of birth pos
                        GridPos centerPos = { 0, 0 };
                        
                        unsigned char *mapChunkMessage = 
                            getChunkMessage( m.x - chunkDimensionX / 2, 
                                             m.y - chunkDimensionY / 2,
                                             chunkDimensionX,
                                             chunkDimensionY,
                                             centerPos,
                                             &length );
                        
                        int numSent = 
                            nextPlayer->sock->send( mapChunkMessage, 
                                                    length, 
                                                    false, false );
                        
                        nextPlayer->gotPartOfThisFrame = true;
                        
                        delete [] mapChunkMessage;

                        if( numSent != length ) {
                            setPlayerDisconnected( nextPlayer, 
                                                   "Socket write failed" );
                            }
                        }
                    else {
                        AppLog::infoF( "Map pull request rejected for %s", 
                                       nextPlayer->email );
                        }
                    }
                else if( m.type == TRIGGER ) {
                    if( areTriggersEnabled() ) {
                        trigger( m.trigger );
                        }
                    }
                else if( m.type == VOGS ) {
                    int allow = 
                        SettingsManager::getIntSetting( "allowVOGMode", 0 );

                    if( allow ) {
                        
                        SimpleVector<char *> *list = 
                            SettingsManager::getSetting( 
                                "vogAllowAccounts" );
                        
                        allow = false;
                        
                        for( int i=0; i<list->size(); i++ ) {
                            if( strcmp( nextPlayer->email,
                                        list->getElementDirect( i ) ) == 0 ) {
                                
                                allow = true;
                                break;
                                }
                            }
                        
                        list->deallocateStringElements();
                        delete list;
                        }
                    

                    if( allow && nextPlayer->connected ) {
                        nextPlayer->vogMode = true;
                        nextPlayer->preVogPos = getPlayerPos( nextPlayer );
                        nextPlayer->preVogBirthPos = nextPlayer->birthPos;
                        nextPlayer->vogJumpIndex = 0;
                        }
                    }
                else if( m.type == VOGN ) {
                    if( nextPlayer->vogMode &&
                        players.size() > 1 ) {
                        
                        nextPlayer->vogJumpIndex++;
                        if( nextPlayer->vogJumpIndex == i ) {
                            nextPlayer->vogJumpIndex++;
                            }
                        if( nextPlayer->vogJumpIndex >= players.size() ) {
                            nextPlayer->vogJumpIndex = 0;
                            }
                        if( nextPlayer->vogJumpIndex == i ) {
                            nextPlayer->vogJumpIndex++;
                            }
                        
                        LiveObject *otherPlayer = 
                            players.getElement( 
                                nextPlayer->vogJumpIndex );
                        
                        GridPos o = getPlayerPos( otherPlayer );
                        
                        GridPos oldPos = getPlayerPos( nextPlayer );
                        

                        nextPlayer->xd = o.x;
                        nextPlayer->yd = o.y;

                        nextPlayer->xs = o.x;
                        nextPlayer->ys = o.y;

                        if( distance( oldPos, o ) > 10000 ) {
                            nextPlayer->birthPos = o;
                            }

                        char *message = autoSprintf( "VU\n%d %d\n#",
                                                     nextPlayer->xs - 
                                                     nextPlayer->birthPos.x,
                                                     nextPlayer->ys -
                                                     nextPlayer->birthPos.y );
                        sendMessageToPlayer( nextPlayer, message,
                                             strlen( message ) );
                        
                        delete [] message;

                        nextPlayer->firstMessageSent = false;
                        nextPlayer->firstMapSent = false;
                        }
                    }
                else if( m.type == VOGP ) {
                    if( nextPlayer->vogMode &&
                        players.size() > 1 ) {

                        nextPlayer->vogJumpIndex--;

                        if( nextPlayer->vogJumpIndex == i ) {
                            nextPlayer->vogJumpIndex--;
                            }
                        if( nextPlayer->vogJumpIndex < 0 ) {
                            nextPlayer->vogJumpIndex = players.size() - 1;
                            }
                        if( nextPlayer->vogJumpIndex == i ) {
                            nextPlayer->vogJumpIndex--;
                            }

                        LiveObject *otherPlayer = 
                            players.getElement( 
                                nextPlayer->vogJumpIndex );
                        
                        GridPos o = getPlayerPos( otherPlayer );
                        
                        GridPos oldPos = getPlayerPos( nextPlayer );
                        

                        nextPlayer->xd = o.x;
                        nextPlayer->yd = o.y;

                        nextPlayer->xs = o.x;
                        nextPlayer->ys = o.y;
                        
                        if( distance( oldPos, o ) > 10000 ) {
                            nextPlayer->birthPos = o;
                            }
                        
                        char *message = autoSprintf( "VU\n%d %d\n#",
                                                     nextPlayer->xs - 
                                                     nextPlayer->birthPos.x,
                                                     nextPlayer->ys -
                                                     nextPlayer->birthPos.y );
                        sendMessageToPlayer( nextPlayer, message,
                                             strlen( message ) );
                        
                        delete [] message;

                        nextPlayer->firstMessageSent = false;
                        nextPlayer->firstMapSent = false;
                        }
                    }
                else if( m.type == VOGM ) {
                    if( nextPlayer->vogMode ) {
                        nextPlayer->xd = m.x;
                        nextPlayer->yd = m.y;
                        
                        nextPlayer->xs = m.x;
                        nextPlayer->ys = m.y;
                        
                        char *message = autoSprintf( "VU\n%d %d\n#",
                                                     nextPlayer->xs - 
                                                     nextPlayer->birthPos.x,
                                                     nextPlayer->ys -
                                                     nextPlayer->birthPos.y );
                        sendMessageToPlayer( nextPlayer, message,
                                             strlen( message ) );
                        
                        delete [] message;
                        }
                    }
                else if( m.type == VOGI ) {
                    if( nextPlayer->vogMode ) {
                        if( m.id > 0 &&
                            getObject( m.id ) != NULL ) {
                            
                            setMapObject( m.x, m.y, m.id );
                            }
                        }
                    }
                else if( m.type == VOGT && m.saidText != NULL ) {
                    if( nextPlayer->vogMode ) {
                        
                        newLocationSpeech.push_back( 
                            stringDuplicate( m.saidText ) );
                        GridPos p = getPlayerPos( nextPlayer );
                        
                        ChangePosition cp;
                        cp.x = p.x;
                        cp.y = p.y;
                        cp.global = false;

                        newLocationSpeechPos.push_back( cp );
                        }
                    }
                else if( m.type == VOGX ) {
                    if( nextPlayer->vogMode ) {
                        nextPlayer->vogMode = false;
                        
                        GridPos p = nextPlayer->preVogPos;
                        
                        nextPlayer->xd = p.x;
                        nextPlayer->yd = p.y;
                        
                        nextPlayer->xs = p.x;
                        nextPlayer->ys = p.y;
                        
                        nextPlayer->birthPos = nextPlayer->preVogBirthPos;

                        // send them one last VU message to move them 
                        // back instantly
                        char *message = autoSprintf( "VU\n%d %d\n#",
                                                     nextPlayer->xs - 
                                                     nextPlayer->birthPos.x,
                                                     nextPlayer->ys -
                                                     nextPlayer->birthPos.y );
                        sendMessageToPlayer( nextPlayer, message,
                                             strlen( message ) );
                        
                        delete [] message;
                        
                        nextPlayer->postVogMode = true;
                        nextPlayer->firstMessageSent = false;
                        nextPlayer->firstMapSent = false;
                        }
                    }
                else if( nextPlayer->vogMode ) {
                    // ignore non-VOG messages from them
                    }
                else if( m.type == FORCE ) {
                    if( m.x == nextPlayer->xd &&
                        m.y == nextPlayer->yd ) {
                        
                        // player has ack'ed their forced pos correctly
                        
                        // stop ignoring their messages now
                        nextPlayer->waitingForForceResponse = false;
                        }
                    else {
                        AppLog::infoF( 
                            "FORCE message has unexpected "
                            "absolute pos (%d,%d), expecting (%d,%d)",
                            m.x, m.y,
                            nextPlayer->xd, nextPlayer->yd );
                        }
                    }
                else if( m.type == PING ) {
                    // immediately send pong
                    char *message = autoSprintf( "PONG\n%d#", m.id );

                    sendMessageToPlayer( nextPlayer, message, 
                                         strlen( message ) );
                    delete [] message;
                    }
                else if( m.type == DIE ) {
                    if( computeAge( nextPlayer ) < 2 ) {
                        
                        // killed self
                        // SID triggers a lineage ban
                        nextPlayer->suicide = true;

                        setDeathReason( nextPlayer, "SID" );

                        nextPlayer->error = true;
                        nextPlayer->errorCauseString = "Baby suicide";
                        int parentID = nextPlayer->parentID;
                        
                        LiveObject *parentO = 
                            getLiveObject( parentID );
                        
                        if( parentO != NULL && nextPlayer->everHeldByParent ) {
                            // mother picked up this SID baby at least
                            // one time
                            // mother can have another baby right away
                            parentO->birthCoolDown = 0;
                            }
                        
                        
                        int holdingAdultID = nextPlayer->heldByOtherID;

                        LiveObject *adult = NULL;
                        if( nextPlayer->heldByOther ) {
                            adult = getLiveObject( holdingAdultID );
                            }

                        int babyBonesID = 
                            SettingsManager::getIntSetting( 
                                "babyBones", -1 );
                        
                        if( adult != NULL ) {
                            
                            if( babyBonesID != -1 ) {
                                ObjectRecord *babyBonesO = 
                                    getObject( babyBonesID );
                                
                                if( babyBonesO != NULL ) {
                                    
                                    // don't leave grave on ground just yet
                                    nextPlayer->customGraveID = 0;
                            
                                    GridPos adultPos = 
                                        getPlayerPos( adult );

                                    // put invisible grave there for now
                                    // find an empty spot for this grave
                                    // where there's no grave already
                                    GridPos gravePos = adultPos;
                                    
                                    // give up after 100 steps
                                    // huge graveyard around?
                                    int stepCount = 0;
                                    while( getGravePlayerID( 
                                               gravePos.x, 
                                               gravePos.y ) > 0 &&
                                           stepCount < 100 ) {
                                        gravePos.x ++;
                                        stepCount ++;
                                        }
                                    
                                    GraveInfo graveInfo = 
                                        { gravePos, 
                                          nextPlayer->id,
                                          nextPlayer->lineageEveID };
                                    newGraves.push_back( graveInfo );
                                    
                                    setGravePlayerID(
                                        gravePos.x, gravePos.y,
                                        nextPlayer->id );
                                    
                                    setHeldGraveOrigin( adult, 
                                                        gravePos.x,
                                                        gravePos.y,
                                                        0 );
                                    
                                    playerIndicesToSendUpdatesAbout.push_back(
                                        getLiveObjectIndex( holdingAdultID ) );
                                    
                                    // what if baby wearing clothes?
                                    for( int c=0; 
                                         c < NUM_CLOTHING_PIECES; 
                                         c++ ) {
                                             
                                        ObjectRecord *cObj = clothingByIndex(
                                            nextPlayer->clothing, c );
                                        
                                        if( cObj != NULL ) {
                                            // put clothing in adult's hand
                                            // and then drop
                                            adult->holdingID = cObj->id;
                                            if( nextPlayer->
                                                clothingContained[c].
                                                size() > 0 ) {
                                                
                                                adult->numContained =
                                                    nextPlayer->
                                                    clothingContained[c].
                                                    size();
                                                
                                                adult->containedIDs =
                                                    nextPlayer->
                                                    clothingContained[c].
                                                    getElementArray();
                                                adult->containedEtaDecays =
                                                    nextPlayer->
                                                    clothingContainedEtaDecays
                                                    [c].
                                                    getElementArray();
                                                
                                                adult->subContainedIDs
                                                    = new 
                                                    SimpleVector<int>[
                                                    adult->numContained ];
                                                adult->subContainedEtaDecays
                                                    = new 
                                                    SimpleVector<timeSec_t>[
                                                    adult->numContained ];
                                                }
                                            
                                            handleDrop( 
                                                adultPos.x, adultPos.y, 
                                                adult,
                                                NULL );
                                            }
                                        }
                                    
                                    // finally leave baby bones
                                    // in their hands
                                    adult->holdingID = babyBonesID;
                                    
                                    // this works to force client to play
                                    // creation sound for baby bones.
                                    adult->heldTransitionSourceID = 
                                        nextPlayer->displayID;
                                    
                                    nextPlayer->heldByOther = false;
                                    }
                                }
                            }
                        else {
                            
                            int babyBonesGroundID = 
                                SettingsManager::getIntSetting( 
                                    "babyBonesGround", -1 );
                            
                            if( babyBonesGroundID != -1 ) {
                                nextPlayer->customGraveID = babyBonesGroundID;
                                }
                            else if( babyBonesID != -1 ) {
                                // else figure out what the held baby bones
                                // become when dropped on ground
                                TransRecord *groundTrans =
                                    getPTrans( babyBonesID, -1 );
                                
                                if( groundTrans != NULL &&
                                    groundTrans->newTarget > 0 ) {
                                    
                                    nextPlayer->customGraveID = 
                                        groundTrans->newTarget;
                                    }
                                }
                            // else just use standard grave
                            }
                        }
                    }
                else if( m.type == GRAVE ) {
                    // immediately send GO response
                    
                    int id = getGravePlayerID( m.x, m.y );
                    
                    DeadObject *o = NULL;
                    for( int i=0; i<pastPlayers.size(); i++ ) {
                        DeadObject *oThis = pastPlayers.getElement( i );
                        
                        if( oThis->id == id ) {
                            o = oThis;
                            break;
                            }
                        }
                    
                    SimpleVector<int> *defaultLineage = 
                        new SimpleVector<int>();
                    
                    defaultLineage->push_back( 0 );
                    DeadObject defaultO = 
                        { 0,
                          0,
                          stringDuplicate( "~" ),
                          defaultLineage,
                          0,
                          0 };
                    
                    if( o == NULL ) {
                        // check for living player too 
                        for( int i=0; i<players.size(); i++ ) {
                            LiveObject *oThis = players.getElement( i );
                            
                            if( oThis->id == id ) {
                                defaultO.id = oThis->id;
                                defaultO.displayID = oThis->displayID;
                            
                                if( oThis->name != NULL ) {
                                    delete [] defaultO.name;
                                    defaultO.name = 
                                        stringDuplicate( oThis->name );
                                    }
                            
                                defaultO.lineage->push_back_other( 
                                    oThis->lineage );
                            
                                defaultO.lineageEveID = oThis->lineageEveID;
                                defaultO.lifeStartTimeSeconds =
                                    oThis->lifeStartTimeSeconds;
                                defaultO.deathTimeSeconds =
                                    oThis->deathTimeSeconds;
                                }
                            }
                        }
                    

                    if( o == NULL ) {
                        o = &defaultO;
                        }

                    if( o != NULL ) {
                        char *formattedName;
                        
                        if( o->name != NULL ) {
                            char found;
                            formattedName =
                                replaceAll( o->name, " ", "_", &found );
                            }
                        else {
                            formattedName = stringDuplicate( "~" );
                            }

                        SimpleVector<char> linWorking;
                        
                        for( int j=0; j<o->lineage->size(); j++ ) {
                            char *mID = 
                                autoSprintf( 
                                    " %d",
                                    o->lineage->getElementDirect( j ) );
                            linWorking.appendElementString( mID );
                            delete [] mID;
                            }
                        char *linString = linWorking.getElementString();
                        
                        double age;
                        
                        if( o->deathTimeSeconds > 0 ) {
                            // "age" in years since they died 
                            age = computeAge( o->deathTimeSeconds );
                            }
                        else {
                            // grave of unknown person
                            // let client know that age is bogus
                            age = -1;
                            }
                        
                        char *message = autoSprintf(
                            "GO\n%d %d %d %d %lf %s%s eve=%d\n#",
                            m.x - nextPlayer->birthPos.x,
                            m.y - nextPlayer->birthPos.y,
                            o->id, o->displayID, 
                            age,
                            formattedName, linString,
                            o->lineageEveID );
                        printf( "Processing %d,%d from birth pos %d,%d\n",
                                m.x, m.y, nextPlayer->birthPos.x,
                                nextPlayer->birthPos.y );
                        
                        delete [] formattedName;
                        delete [] linString;

                        sendMessageToPlayer( nextPlayer, message, 
                                             strlen( message ) );
                        delete [] message;
                        }
                    
                    delete [] defaultO.name;
                    delete defaultO.lineage;
                    }
                else if( m.type == OWNER ) {
                    // immediately send OW response
                    SimpleVector<char> messageWorking;
                    messageWorking.appendElementString( "OW\n" );
                    
                    char *leadString = 
                        autoSprintf( "%d %d", 
                                     m.x - nextPlayer->birthPos.x, 
                                     m.y - nextPlayer->birthPos.y );
                    messageWorking.appendElementString( leadString );
                    delete [] leadString;
                    
                    char *ownerString = getOwnershipString( m.x, m.y );
                    messageWorking.appendElementString( ownerString );
                    delete [] ownerString;

                    messageWorking.appendElementString( "\n#" );
                    char *message = messageWorking.getElementString();
                    
                    sendMessageToPlayer( nextPlayer, message, 
                                         strlen( message ) );
                    delete [] message;

                    GridPos p = { m.x, m.y };
                    
                    if( ! isKnownOwned( nextPlayer, p ) ) {
                        // remember that we know about it
                        nextPlayer->knownOwnedPositions.push_back( p );
                        }
                    }
                else if( m.type == PHOTO ) {
                    // immediately send photo response

                    char *photoServerSharedSecret = 
                        SettingsManager::
                        getStringSetting( "photoServerSharedSecret",
                                          "secret_phrase" );
                    
                    char *idString = autoSprintf( "%d", m.id );
                    
                    char *hash;
                    
                    // is a photo device present at x and y?
                    char photo = false;
                    
                    int oID = getMapObject( m.x, m.y );
                    
                    if( oID > 0 ) {
                        if( strstr( getObject( oID )->description,
                                    "+photo" ) != NULL ) {
                            photo = true;
                            }
                        }
                    
                    if( ! photo ) {
                        hash = hmac_sha1( "dummy", idString );
                        }
                    else {
                        hash = hmac_sha1( photoServerSharedSecret, idString );
                        }
                    
                    delete [] photoServerSharedSecret;
                    delete [] idString;
                    
                    char *message = autoSprintf( "PH\n%d %d %s#", 
                                                 m.x, m.y, hash );
                    
                    delete [] hash;

                    sendMessageToPlayer( nextPlayer, message, 
                                         strlen( message ) );
                    delete [] message;
                    }

                else if( m.type != SAY && m.type != EMOT &&
                         nextPlayer->waitingForForceResponse ) {
                    // if we're waiting for a FORCE response, ignore
                    // all messages from player except SAY and EMOT
                    
                    AppLog::infoF( "Ignoring client message because we're "
                                   "waiting for FORCE ack message after a "
                                   "forced-pos PU at (%d, %d), "
                                   "relative=(%d, %d)",
                                   nextPlayer->xd, nextPlayer->yd,
                                   nextPlayer->xd - nextPlayer->birthPos.x,
                                   nextPlayer->yd - nextPlayer->birthPos.y );
                    }
                // if player is still moving (or held by an adult), 
                // ignore all actions
                // except for move interrupts
                else if( ( nextPlayer->xs == nextPlayer->xd &&
                           nextPlayer->ys == nextPlayer->yd &&
                           ! nextPlayer->heldByOther )
                         ||
                         m.type == MOVE ||
                         m.type == JUMP || 
                         m.type == SAY ||
                         m.type == EMOT ) {

                    if( m.type == MOVE &&
                        m.sequenceNumber != -1 ) {
                        nextPlayer->lastMoveSequenceNumber = m.sequenceNumber;
                        }

                    if( ( m.type == MOVE || m.type == JUMP ) && 
                        nextPlayer->heldByOther ) {
                        
                        // only JUMP actually makes them jump out
                        if( m.type == JUMP ) {
                            // baby wiggling out of parent's arms
                            
                            // block them from wiggling from their own 
                            // mother's arms if they are under 1
                            
                            if( computeAge( nextPlayer ) >= 1  ||
                                nextPlayer->heldByOtherID != 
                                nextPlayer->parentID ) {
                                
                                handleForcedBabyDrop( 
                                    nextPlayer,
                                    &playerIndicesToSendUpdatesAbout );
                                }
                            else {
                                // baby wiggles
                                nextPlayer->wiggleUpdate = true;
                                }
                            }
                        
                        // ignore their move requests while
                        // in-arms, until they JUMP out
                        }
                    else if( m.type == JUMP &&
                             computeAge( nextPlayer ) < startWalkingAge ) {
                        // tiny infant wiggling on ground
                        nextPlayer->wiggleUpdate = true;
                        }
                    else if( m.type == MOVE && 
                             computeAge( nextPlayer ) < startWalkingAge ) {
                        // ignore moves for the tiniest infants
                        }
                    else if( m.type == MOVE && nextPlayer->holdingID > 0 &&
                             getObject( nextPlayer->holdingID )->
                             speedMult == 0 ) {
                        // next player holding something that prevents
                        // movement entirely
                        printf( "  Processing move, "
                                "but player holding a speed-0 object, "
                                "ending now\n" );
                        nextPlayer->xd = nextPlayer->xs;
                        nextPlayer->yd = nextPlayer->ys;
                        
                        nextPlayer->posForced = true;
                        
                        // send update about them to end the move
                        // right now
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        }
                    else if( m.type == MOVE ) {
                        //Thread::staticSleep( 1000 );

                        /*
                        printf( "  Processing move, "
                                "we think player at old start pos %d,%d\n",
                                nextPlayer->xs,
                                nextPlayer->ys );
                        printf( "  Player's last path = " );
                        for( int p=0; p<nextPlayer->pathLength; p++ ) {
                            printf( "(%d, %d) ",
                                    nextPlayer->pathToDest[p].x, 
                                    nextPlayer->pathToDest[p].y );
                            }
                        printf( "\n" );
                        */
                        
                        char interrupt = false;
                        char pathPrefixAdded = false;
                        

                        // where exactly did we used to be standing?
                        doublePair startPosPrecise =
                            computePartialMoveSpotPrecise( nextPlayer );
                                

                        // first, construct a path from any existing
                        // path PLUS path that player is suggesting
                        SimpleVector<GridPos> unfilteredPath;

                        if( nextPlayer->pathLength > 0 &&
                            nextPlayer->pathToDest != NULL &&
                            ( nextPlayer->xs != m.x ||
                              nextPlayer->ys != m.y ) ) {
                            
                            // start pos of their submitted path
                            // donesn't match where we think they are

                            // it could be an interrupt to past move
                            // OR, if our server sees move as done but client 
                            // doesn't yet, they may be sending a move
                            // from the middle of their last path.

                            // treat this like an interrupt to last move
                            // in both cases.

                            // a new move interrupting a non-stationary object
                            interrupt = true;

                            // where we think they are along last move path
                            GridPos cPos;
                            int c;
                            
                            if( nextPlayer->xs != nextPlayer->xd 
                                ||
                                nextPlayer->ys != nextPlayer->yd ) {
                                
                                // a real interrupt to a move that is
                                // still in-progress on server
                                c = computePartialMovePathStep( nextPlayer );
                                cPos = computePartialMoveSpot( nextPlayer, c );
                                }
                            else {
                                // we think their last path is done
                                cPos.x = nextPlayer->xs;
                                cPos.y = nextPlayer->ys;
                                // we think they are on final destination
                                // spot on last path
                                c = nextPlayer->pathLength - 1;
                                }
                            
                            /*
                            printf( "   we think player in motion or "
                                    "done moving at %d,%d\n",
                                    cPos.x,
                                    cPos.y );
                            */
                            nextPlayer->xs = cPos.x;
                            nextPlayer->ys = cPos.y;
                            
                            
                            char cOnTheirNewPath = false;
                            

                            for( int p=0; p<m.numExtraPos; p++ ) {
                                if( equal( cPos, m.extraPos[p] ) ) {
                                    cOnTheirNewPath = true;
                                    break;
                                    }
                                }
                            
                            if( cPos.x == m.x && cPos.y == m.y ) {
                                // also if equal to their start pos
                                cOnTheirNewPath = true;
                                }
                            


                            if( !cOnTheirNewPath &&
                                nextPlayer->pathLength > 0 ) {

                                // add prefix to their path from
                                // c to the start of their path
                                
                                // index where they think they are

                                // could be ahead or behind where we think
                                // they are
                                
                                int theirPathIndex = -1;
                            
                                for( int p=0; p<nextPlayer->pathLength; p++ ) {
                                    GridPos pos = nextPlayer->pathToDest[p];
                                    
                                    if( m.x == pos.x && m.y == pos.y ) {
                                        // reached point along old path
                                        // where player thinks they 
                                        // actually are
                                        theirPathIndex = p;
                                        break;
                                        }
                                    }
                                
                                char theirIndexNotFound = false;
                                
                                if( theirPathIndex == -1 ) {
                                    // if not found, assume they think they
                                    // are at start of their old path
                                    
                                    theirIndexNotFound = true;
                                    theirPathIndex = 0;
                                    }
                                
                                /*
                                printf( "They are on our path at index %d\n",
                                        theirPathIndex );
                                */

                                // okay, they think they are on last path
                                // that we had for them

                                // step through path from where WE
                                // think they should be to where they
                                // think they are and add this as a prefix
                                // to the path they submitted
                                // (we may walk backward along the old
                                //  path to do this)
                                

                                // -1 means starting, pre-path 
                                // pos is closest
                                // but okay to leave c at -1, because
                                // we will add pathStep=1 to it

                                int pathStep = 0;
                                    
                                if( theirPathIndex < c ) {
                                    pathStep = -1;
                                    }
                                else if( theirPathIndex > c ) {
                                    pathStep = 1;
                                    }
                                    
                                if( pathStep != 0 ) {

                                    if( c == -1 ) {
                                        // fix weird case where our start
                                        // pos is on our path
                                        // not sure what causes this
                                        // but it causes the valid path
                                        // check to fail below
                                        int firstStep = c + pathStep;
                                        GridPos firstPos =
                                            nextPlayer->pathToDest[ firstStep ];
                                        
                                        if( firstPos.x == nextPlayer->xs &&
                                            firstPos.y == nextPlayer->ys ) {
                                            c = 0;
                                            }
                                        }
                                    
                                    for( int p = c + pathStep; 
                                         p != theirPathIndex + pathStep; 
                                         p += pathStep ) {
                                        GridPos pos = 
                                            nextPlayer->pathToDest[p];
                                            
                                        unfilteredPath.push_back( pos );
                                        }
                                    }

                                if( theirIndexNotFound ) {
                                    // add their path's starting pos
                                    // at the end of the prefix
                                    GridPos pos = { m.x, m.y };
                                    
                                    unfilteredPath.push_back( pos );
                                    }
                                
                                // otherwise, they are where we think
                                // they are, and we don't need to prefix
                                // their path

                                /*
                                printf( "Prefixing their path "
                                        "with %d steps\n",
                                        unfilteredPath.size() );
                                */
                                }
                            }
                        
                        if( unfilteredPath.size() > 0 ) {
                            pathPrefixAdded = true;
                            }

                        // now add path player says they want to go down

                        for( int p=0; p < m.numExtraPos; p++ ) {
                            unfilteredPath.push_back( m.extraPos[p] );
                            }
                        
                        /*
                        printf( "Unfiltered path = " );
                        for( int p=0; p<unfilteredPath.size(); p++ ) {
                            printf( "(%d, %d) ",
                                    unfilteredPath.getElementDirect(p).x, 
                                    unfilteredPath.getElementDirect(p).y );
                            }
                        printf( "\n" );
                        */

                        // remove any duplicate spots due to doubling back

                        for( int p=1; p<unfilteredPath.size(); p++ ) {
                            
                            if( equal( unfilteredPath.getElementDirect(p-1),
                                       unfilteredPath.getElementDirect(p) ) ) {
                                unfilteredPath.deleteElement( p );
                                p--;
                                //printf( "FOUND duplicate path element\n" );
                                }
                            }
                            
                                
                                       
                        
                        nextPlayer->xd = m.extraPos[ m.numExtraPos - 1].x;
                        nextPlayer->yd = m.extraPos[ m.numExtraPos - 1].y;
                        
                        
                        if( nextPlayer->xd == nextPlayer->xs &&
                            nextPlayer->yd == nextPlayer->ys ) {
                            // this move request truncates to where
                            // we think player actually is

                            // send update to terminate move right now
                            playerIndicesToSendUpdatesAbout.push_back( i );
                            /*
                            printf( "A move that takes player "
                                    "where they already are, "
                                    "ending move now\n" );
                            */
                            }
                        else {
                            // an actual move away from current xs,ys

                            if( interrupt ) {
                                //printf( "Got valid move interrupt\n" );
                                }
                                

                            // check path for obstacles
                            // and make sure it contains the location
                            // where we think they are
                            
                            char truncated = 0;
                            
                            SimpleVector<GridPos> validPath;

                            char startFound = false;
                            
                            
                            int startIndex = 0;
                            // search from end first to find last occurrence
                            // of start pos
                            for( int p=unfilteredPath.size() - 1; p>=0; p-- ) {
                                
                                if( unfilteredPath.getElementDirect(p).x 
                                      == nextPlayer->xs
                                    &&
                                    unfilteredPath.getElementDirect(p).y 
                                      == nextPlayer->ys ) {
                                    
                                    startFound = true;
                                    startIndex = p;
                                    break;
                                    }
                                }
                            /*
                            printf( "Start index = %d (startFound = %d)\n", 
                                    startIndex, startFound );
                            */

                            if( ! startFound &&
                                ! isGridAdjacentDiag( 
                                    unfilteredPath.
                                      getElementDirect(startIndex).x,
                                    unfilteredPath.
                                      getElementDirect(startIndex).y,
                                    nextPlayer->xs,
                                    nextPlayer->ys ) ) {
                                // path start jumps away from current player 
                                // start
                                // ignore it
                                }
                            else {
                                
                                GridPos lastValidPathStep =
                                    { m.x, m.y };
                                
                                if( pathPrefixAdded ) {
                                    lastValidPathStep.x = nextPlayer->xs;
                                    lastValidPathStep.y = nextPlayer->ys;
                                    }
                                
                                // we know where we think start
                                // of this path should be,
                                // but player may be behind this point
                                // on path (if we get their message late)
                                // So, it's not safe to pre-truncate
                                // the path

                                // However, we will adjust timing, below,
                                // to match where we think they should be
                                
                                // enforce client behavior of not walking
                                // down through objects in our cell that are
                                // blocking us
                                char currentBlocked = false;
                                
                                if( isMapSpotBlocking( lastValidPathStep.x,
                                                       lastValidPathStep.y ) ) {
                                    currentBlocked = true;
                                    }
                                

                                for( int p=0; 
                                     p<unfilteredPath.size(); p++ ) {
                                
                                    GridPos pos = 
                                        unfilteredPath.getElementDirect(p);

                                    if( isMapSpotBlocking( pos.x, pos.y ) ) {
                                        // blockage in middle of path
                                        // terminate path here
                                        truncated = 1;
                                        break;
                                        }
                                    
                                    if( currentBlocked && p == 0 &&
                                        pos.y == lastValidPathStep.y - 1 ) {
                                        // attempt to walk down through
                                        // blocking object at starting location
                                        truncated = 1;
                                        break;
                                        }
                                    

                                    // make sure it's not more
                                    // than one step beyond
                                    // last step

                                    if( ! isGridAdjacentDiag( 
                                            pos, lastValidPathStep ) ) {
                                        // a path with a break in it
                                        // terminate it here
                                        truncated = 1;
                                        break;
                                        }
                                    
                                    // no blockage, no gaps, add this step
                                    validPath.push_back( pos );
                                    lastValidPathStep = pos;
                                    }
                                }
                            
                            if( validPath.size() == 0 ) {
                                // path not permitted
                                AppLog::info( "Path submitted by player "
                                              "not valid, "
                                              "ending move now" );
                                //assert( false );
                                nextPlayer->xd = nextPlayer->xs;
                                nextPlayer->yd = nextPlayer->ys;
                                
                                nextPlayer->posForced = true;

                                // send update about them to end the move
                                // right now
                                playerIndicesToSendUpdatesAbout.push_back( i );
                                }
                            else {
                                // a good path
                                
                                if( nextPlayer->pathToDest != NULL ) {
                                    delete [] nextPlayer->pathToDest;
                                    nextPlayer->pathToDest = NULL;
                                    }

                                nextPlayer->pathTruncated = truncated;
                                
                                nextPlayer->pathLength = validPath.size();
                                
                                nextPlayer->pathToDest = 
                                    validPath.getElementArray();
                                    
                                // path may be truncated from what was 
                                // requested, so set new d
                                nextPlayer->xd = 
                                    nextPlayer->pathToDest[ 
                                        nextPlayer->pathLength - 1 ].x;
                                nextPlayer->yd = 
                                    nextPlayer->pathToDest[ 
                                        nextPlayer->pathLength - 1 ].y;

                                // distance is number of orthogonal steps
                            
                                double dist = 
                                    measurePathLength( nextPlayer->xs,
                                                       nextPlayer->ys,
                                                       nextPlayer->pathToDest,
                                                       nextPlayer->pathLength );
 
                                nextPlayer->pathDist = dist;

                                
                                // get precise about distance for move timing
                                // we don't necessarily start right
                                // at naiveStart, but often some distance
                                // along (or further behind)
                                GridPos naiveStart;
                                
                                if( startIndex > 0 ) {
                                    naiveStart = 
                                        nextPlayer->pathToDest[ startIndex -1 ];
                                    }
                                else {
                                    naiveStart.x = nextPlayer->xs;
                                    naiveStart.y = nextPlayer->ys;
                                    }
                                
                                double naiveStartDist = 
                                    distance( 
                                        naiveStart,
                                        nextPlayer->pathToDest[startIndex] );
                                
                                // subtract out this naive
                                // first-step distance
                                // before adding in the true distance
                                dist -= naiveStartDist;
                                
                                doublePair newFirstSpot = 
                                    { (double)
                                      nextPlayer->pathToDest[startIndex].x,
                                      (double)
                                      nextPlayer->pathToDest[startIndex].y };
                                
                                        
                                // now add in true distance to first spot
                                dist +=
                                    distance( startPosPrecise, newFirstSpot );
  

                                

                                double distAlreadyDone =
                                    measurePathLength( nextPlayer->xs,
                                                       nextPlayer->ys,
                                                       nextPlayer->pathToDest,
                                                       startIndex );
                             
                                double moveSpeed = computeMoveSpeed( 
                                    nextPlayer ) *
                                    getPathSpeedModifier( 
                                        nextPlayer->pathToDest,
                                        nextPlayer->pathLength );
                                
                                nextPlayer->moveTotalSeconds = dist / 
                                    moveSpeed;
                                
                                if( nextPlayer->moveTotalSeconds <= 0.1 ) {
                                    // never allow moveTotalSeconds to be
                                    // 0, too small, or negative
                                    // (we divide by it in certain 
                                    // calculations)
                                    nextPlayer->moveTotalSeconds = 0.1;
                                    }
                                
                                double secondsAlreadyDone = distAlreadyDone / 
                                    moveSpeed;
                                /*
                                printf( "Skipping %f seconds along new %f-"
                                        "second path\n",
                                        secondsAlreadyDone, 
                                        nextPlayer->moveTotalSeconds );
                                */
                                nextPlayer->moveStartTime = 
                                    Time::getCurrentTime() - 
                                    secondsAlreadyDone;
                            
                                nextPlayer->newMove = true;
                                
                                
                                // check if path passes over
                                // an object with autoDefaultTrans
                                for( int p=0; p< nextPlayer->pathLength; p++ ) {
                                    int x = nextPlayer->pathToDest[p].x;
                                    int y = nextPlayer->pathToDest[p].y;
                                    
                                    int oID = getMapObject( x, y );
                                    
                                    if( oID > 0 &&
                                        getObject( oID )->autoDefaultTrans ) {
                                        TransRecord *t = getPTrans( -2, oID );
                                        
                                        if( t == NULL ) {
                                            // also consider applying bare-hand
                                            // action, if defined and if
                                            // it produces nothing in the hand
                                            t = getPTrans( 0, oID );
                                            
                                            if( t != NULL &&
                                                t->newActor > 0 ) {
                                                t = NULL;
                                                }
                                            }

                                        if( t != NULL && t->newTarget > 0 ) {
                                            int newTarg = t->newTarget;
                                            setMapObject( x, y, newTarg );

                                            TransRecord *timeT =
                                                getPTrans( -1, newTarg );
                                            
                                            if( timeT != NULL &&
                                                timeT->autoDecaySeconds < 20 ) {
                                                // target will decay to
                                                // something else in a short
                                                // time
                                                // Likely meant to reset
                                                // after person passes through
                                                
                                                // fix the time based on our
                                                // pass-through time
                                                double timeLeft =
                                                    nextPlayer->moveTotalSeconds
                                                    - secondsAlreadyDone;
                                                
                                                double plannedETADecay =
                                                    Time::getCurrentTime()
                                                    + timeLeft 
                                                    // pad with extra second
                                                    + 1;
                                                
                                                timeSec_t actual =
                                                    getEtaDecay( x, y );
                                                
                                                // don't ever shorten
                                                // we could be interrupting
                                                // another player who
                                                // is on a longer path
                                                // through the same object
                                                if( plannedETADecay >
                                                    actual ) {
                                                    setEtaDecay( 
                                                        x, y, plannedETADecay );
                                                    }
                                                }
                                            }
                                        }
                                    }


                                
                                // check if this move goes into a bad biome
                                // and makes them sick
                                int sicknessObjectID = -1;
                                
                                
                                for( int p=0; p< nextPlayer->pathLength; p++ ) {
                                    
                                    sicknessObjectID = 
                                        getBiomeSickness( 
                                            nextPlayer->displayID, 
                                            nextPlayer->pathToDest[p].x,
                                            nextPlayer->pathToDest[p].y );

                                    if( sicknessObjectID != -1 ) {
                                        break;
                                        }
                                    }
                                
                                
                                if( nextPlayer->vogMode || 
                                    nextPlayer->forceSpawn ||
                                    nextPlayer->isTutorial ) {
                                    // these special-case players never
                                    // have biome sickness
                                    sicknessObjectID = -1;
                                    }
                                
                                // riding something prevents sickness
                                if( sicknessObjectID > 0 &&
                                    nextPlayer->holdingID > 0 &&
                                    getObject( nextPlayer->holdingID )->
                                    rideable ) {
                                    
                                    sicknessObjectID = -1;
                                    }

                                if( sicknessObjectID > 0 &&
                                    ! nextPlayer->holdingWound &&
                                    nextPlayer->holdingID != 
                                    sicknessObjectID ) {
                                    
                                    // drop what they are holding
                                    if( nextPlayer->holdingID != 0 ) {
                                        // never drop held wounds
                                        // or neverDrop murder weapons

                                        if( ! nextPlayer->holdingWound &&
                                            ! nextPlayer->
                                            holdingBiomeSickness &&
                                            ! heldNeverDrop( nextPlayer ) ) {
                                            handleDrop( 
                                             m.x, m.y, nextPlayer,
                                             &playerIndicesToSendUpdatesAbout );
                                            }
                                        }
                                    
                                    if( nextPlayer->holdingID == 0 ||
                                        nextPlayer->holdingBiomeSickness ) {
                                        // we dropped what they were holding
                                        // or they were holding a different
                                        // biome sickness, which we can now
                                        // freely replace
                                        
                                        nextPlayer->holdingID = 
                                            sicknessObjectID;
                                        playerIndicesToSendUpdatesAbout.
                                            push_back( i );

                                        nextPlayer->holdingBiomeSickness = true;

                                        ForcedEffects e = 
                                            checkForForcedEffects( 
                                                nextPlayer->holdingID );
                            
                                        if( e.emotIndex != -1 ) {
                                            nextPlayer->emotFrozen = true;
                                            nextPlayer->emotFrozenIndex =
                                                e.emotIndex;
                                            newEmotPlayerIDs.push_back( 
                                                nextPlayer->id );
                                            newEmotIndices.push_back( 
                                                e.emotIndex );
                                            newEmotTTLs.push_back( e.ttlSec );
                                            interruptAnyKillEmots( 
                                                nextPlayer->id, e.ttlSec );
                                            }
                                        }
                                    }
                                else if( sicknessObjectID == -1 &&
                                         nextPlayer->holdingBiomeSickness ) {
                                    
                                    endBiomeSickness( 
                                        nextPlayer, i,
                                        &playerIndicesToSendUpdatesAbout );
                                    }
                                }
                            }
                        }
                    else if( m.type == SAY && m.saidText != NULL &&
                             Time::getCurrentTime() - 
                             nextPlayer->lastSayTimeSeconds > 
                             minSayGapInSeconds ) {
                        
                        nextPlayer->lastSayTimeSeconds = 
                            Time::getCurrentTime();

                        unsigned int sayLimit = getSayLimit( nextPlayer );
                        
                        if( strlen( m.saidText ) > sayLimit ) {
                            // truncate
                            m.saidText[ sayLimit ] = '\0';
                            }

                        int len = strlen( m.saidText );
                        
                        // replace not-allowed characters with spaces
                        for( int c=0; c<len; c++ ) {
                            if( ! allowedSayCharMap[ 
                                    (int)( m.saidText[c] ) ] ) {
                                
                                m.saidText[c] = ' ';
                                }
                            }

                        
                        if( nextPlayer->ownedPositions.size() > 0 ) {
                            // consider phrases that assign ownership
                            SimpleVector<LiveObject*> newOwners;
                            

                            char *namedOwner = isNamedGivingSay( m.saidText );
                            
                            if( namedOwner != NULL ) {
                                LiveObject *o =
                                    getPlayerByName( namedOwner, nextPlayer );
                                
                                if( o != NULL ) {
                                    newOwners.push_back( o );
                                    }
                                delete [] namedOwner;
                                }

                            if( newOwners.size() == 0 ) {
                                
                                if( isYouGivingSay( m.saidText ) ) {
                                    // find closest other player
                                    LiveObject *newOwnerPlayer = 
                                        getClosestOtherPlayer( nextPlayer );
                                    
                                    if( newOwnerPlayer != NULL ) {
                                        newOwners.push_back( newOwnerPlayer );
                                        }
                                    }
                                else if( isFamilyGivingSay( m.saidText ) ) {
                                    // add all family members
                                    for( int n=0; n<players.size(); n++ ) {
                                        LiveObject *o = players.getElement( n );
                                        if( o->error || 
                                            o->id == nextPlayer->id ) {
                                            continue;
                                            }
                                        if( o->lineageEveID == 
                                            nextPlayer->lineageEveID ) {
                                            newOwners.push_back( o );
                                            }
                                        }
                                    }
                                else if( isOffspringGivingSay( m.saidText ) ) {
                                    // add all offspring
                                    for( int n=0; n<players.size(); n++ ) {
                                        LiveObject *o = players.getElement( n );
                                        if( o->error || 
                                            o->id == nextPlayer->id ) {
                                            continue;
                                            }
                                        if( o->parentID == nextPlayer->id ) {
                                            newOwners.push_back( o );
                                            }
                                        }
                                    }
                                }
                            
                            
                            if( newOwners.size() > 0 ) {
                                // find closest spot that this player owns
                                GridPos thisPos = getPlayerPos( nextPlayer );

                                double minDist = DBL_MAX;
                            
                                GridPos closePos;
                            
                                for( int j=0; 
                                     j< nextPlayer->ownedPositions.size();
                                     j++ ) {
                                    GridPos nextPos = 
                                        nextPlayer->
                                        ownedPositions.getElementDirect( j );
                                    double d = distance( nextPos, thisPos );
                                
                                    if( d < minDist ) {
                                        minDist = d;
                                        closePos = nextPos;
                                        }
                                    }

                                if( minDist < DBL_MAX ) {
                                    // found one
                                    for( int n=0; n<newOwners.size(); n++ ) {
                                        LiveObject *newOwnerPlayer = 
                                            newOwners.getElementDirect( n );
                                        
                                        if( ! isOwned( newOwnerPlayer, 
                                                       closePos ) ) {
                                            newOwnerPlayer->
                                                ownedPositions.push_back( 
                                                    closePos );
                                            newOwnerPos.push_back( closePos );
                                            }
                                        }
                                    }
                                }
                            }


                        // they must be holding something to join a posse
                        if( nextPlayer->holdingID > 0 && 
                            isPosseJoiningSay( m.saidText ) ) {
                            
                            GridPos ourPos = getPlayerPos( nextPlayer );
                            
                            // find closest player who is part of a KILL
                            // record
                            KillState *closestState = NULL;
                            double closestDist = DBL_MAX;
                            for( int i=0; i<activeKillStates.size(); i++ ) {
                                KillState *s = activeKillStates.getElement( i );

                                if( s->targetID == nextPlayer->id ) {
                                    // can't join posse targetting self
                                    continue;
                                    }
                                if( s->killerID == nextPlayer->id ) {
                                    // can't join posse that we're already in
                                    continue;
                                    }
                                
                                LiveObject *killer = 
                                    getLiveObject( s->killerID );
                                
                                GridPos killerPos = getPlayerPos( killer );
                                
                                double d = distance( killerPos, ourPos );
                                
                                if( d < 8 &&
                                    d < closestDist ) {
                                    // in range and closer
                                    closestState = s;
                                    closestDist = d;
                                    }
                                }

                            if( closestState != NULL &&
                                ! isAlreadyInKillState( nextPlayer ) ) {
                                // they are joining, and they aren't already
                                // in one.
                                // infinite range
                                removeAnyKillState( nextPlayer );
                                
                                char enteredState = addKillState( 
                                    nextPlayer, 
                                    getLiveObject( closestState->targetID ),
                                    true ); 
                                if( enteredState ) {
                                    nextPlayer->emotFrozen = true;
                                    nextPlayer->emotFrozenIndex = 
                                        killEmotionIndex;
                                    
                                    newEmotPlayerIDs.push_back( 
                                        nextPlayer->id );
                                    newEmotIndices.push_back( 
                                        killEmotionIndex );
                                    newEmotTTLs.push_back( 120 );
                                    }
                                }
                            }
                        

                        
                        if( nextPlayer->isEve && nextPlayer->name == NULL ) {
                            char *name = isFamilyNamingSay( m.saidText );
                            
                            if( name != NULL && strcmp( name, "" ) != 0 ) {
                                nameEve( nextPlayer, name );
                                playerIndicesToSendNamesAbout.push_back( i );
                                replaceNameInSaidPhrase( 
                                    name,
                                    &( m.saidText ),
                                    nextPlayer, true );
                                
                                if( ! isEveWindow() ) {
                                    // new family name created
                                    restockPostWindowFamilies();
                                    }        

                                if( strstr( m.saidText, "EVE EVE" ) != NULL ) {
                                    // their naming phrase was I AM EVE SMITH
                                    // already
                                    char found;
                                    char *fixed =
                                        replaceOnce( m.saidText, 
                                                     "EVE EVE",
                                                     "EVE",
                                                     &found );
                                    delete [] m.saidText;
                                    m.saidText = fixed;
                                    }
                                }
                            }

                        
                        LiveObject *otherToFollow = NULL;
                        LiveObject *otherToExile = NULL;
                        LiveObject *otherToRedeem = NULL;
                        
                        if( isYouFollowSay( m.saidText ) ) {
                            otherToFollow = getClosestOtherPlayer( nextPlayer );
                            }
                        else {
                           char *namedPlayer = isNamedFollowSay( m.saidText );
                            
                           if( namedPlayer != NULL ) {
                               printf( "Named player = '%s\n", namedPlayer );
                               otherToFollow =
                                   getPlayerByName( namedPlayer, nextPlayer );
                               
                               if( otherToFollow == NULL &&
                                   ( strcmp( namedPlayer, "MYSELF" ) == 0 ||
                                     strcmp( namedPlayer, "NO ONE" ) == 0 ||
                                     strcmp( namedPlayer, "NOBODY" ) == 0 ) ) {
                                   otherToFollow = nextPlayer;
                                   }
                               }
                            }
                        
                        if( otherToFollow != NULL ) {
                            if( otherToFollow == nextPlayer ) {
                                if( nextPlayer->followingID != -1 ) {
                                    nextPlayer->followingID = -1;
                                    nextPlayer->followingUpdate = true;
                                    }
                                }
                            else if( nextPlayer->followingID != 
                                     otherToFollow->id ) {
                                nextPlayer->followingID = otherToFollow->id;
                                nextPlayer->followingUpdate = true;
                                
                                if( otherToFollow->leadingColorIndex == -1 ) {
                                    otherToFollow->leadingColorIndex =
                                        getUnusedLeadershipColor();
                                    }

                                // break any loops
                                LiveObject *o = nextPlayer;
                                
                                while( o != NULL && o->followingID != -1 ) {
                                    if( o->followingID == nextPlayer->id ) {
                                        // loop
                                        // break it by having next player's
                                        // new leader follow no one
                                        otherToFollow->followingID = -1;
                                        otherToFollow->followingUpdate = true;
                                        break;
                                        }
                                    o = getLiveObject( o->followingID );
                                    }
                                }
                            }
                        else {
                            if( isYouExileSay( m.saidText ) ) {
                                otherToExile = 
                                    getClosestOtherPlayer( nextPlayer );
                                }
                            else {
                                char *namedPlayer = 
                                    isNamedExileSay( m.saidText );
                            
                                if( namedPlayer != NULL ) {
                                    otherToExile =
                                        getPlayerByName( namedPlayer, 
                                                         nextPlayer );
                                    }
                                }
                            
                            if( otherToExile != NULL ) {
                                if( otherToExile->
                                    exiledByIDs.getElementIndex( 
                                        nextPlayer->id ) == -1 ) {
                                    otherToExile->exiledByIDs.push_back(
                                        nextPlayer->id );

                                    otherToExile->exileUpdate = true;
                                    }
                                }
                            else {
                                if( isYouRedeemSay( m.saidText ) ) {
                                    otherToRedeem = 
                                        getClosestOtherPlayer( nextPlayer );
                                    }
                                else {
                                    char *namedPlayer = 
                                        isNamedRedeemSay( m.saidText );
                                    
                                    if( namedPlayer != NULL ) {
                                        otherToRedeem =
                                            getPlayerByName( namedPlayer, 
                                                             nextPlayer );
                                        }
                                    }
                            
                                if( otherToRedeem != NULL ) {
                                    // pass redemption downward
                                    // clearing up exiles perpetrated by
                                    // our followers
                                    
                                    for( int e=0; 
                                         e<otherToRedeem->exiledByIDs.size();
                                         e++ ) {
                                        
                                        // for
                                        LiveObject *exiler =
                                            getLiveObject( 
                                                otherToRedeem->
                                                exiledByIDs.
                                                getElementDirect( e ) );
                                        
                                        if( exiler == nextPlayer ) {
                                            
                                            otherToRedeem->
                                                exiledByIDs.deleteElement( e );
                                            e--;
                                            otherToRedeem->exileUpdate = true;
                                            }
                                        else if( exiler != NULL && 
                                                 isFollower( nextPlayer,
                                                             exiler ) ) {
                                            otherToRedeem->
                                                exiledByIDs.deleteElement( e );
                                            e--;
                                            otherToRedeem->exileUpdate = true;
                                            }
                                        }
                                    }
                                }
                            }
                        
                        if( nextPlayer->holdingID > 0 &&
                            getObject( nextPlayer->holdingID )->deadlyDistance
                            > 0 ) {
                            // are they speaking intent to kill?

                            LiveObject *otherToKill = NULL;
                            
                            if( isYouKillSay( m.saidText ) ) {
                                otherToKill = 
                                    getClosestOtherPlayer( nextPlayer );
                                }
                            else {
                                char *namedPlayer = 
                                    isNamedKillSay( m.saidText );
                                    
                                if( namedPlayer != NULL ) {
                                    otherToKill =
                                        getPlayerByName( namedPlayer, 
                                                         nextPlayer );
                                    delete [] namedPlayer;
                                    }
                                }

                            if( otherToKill != NULL ) {
                                playerIndicesToSendUpdatesAbout.push_back( i );
                                tryToStartKill( nextPlayer, otherToKill->id );
                                }
                            }
                        


                        if( nextPlayer->holdingID < 0 ) {

                            // we're holding a baby
                            // (no longer matters if it's our own baby)
                            // (we let adoptive parents name too)
                            
                            LiveObject *babyO =
                                getLiveObject( - nextPlayer->holdingID );
                            
                            if( babyO != NULL && babyO->name == NULL ) {
                                char *name = isBabyNamingSay( m.saidText );

                                if( name != NULL && strcmp( name, "" ) != 0 ) {
                                    nameBaby( nextPlayer, babyO, name,
                                              &playerIndicesToSendNamesAbout );
                                    replaceNameInSaidPhrase( name,
                                                             &( m.saidText ),
                                                             babyO );
                                    }
                                }
                            }
                        else {
                            // not holding anyone
                        
                            char *name = isBabyNamingSay( m.saidText );
                                
                            if( name != NULL && strcmp( name, "" ) != 0 ) {
                                // still, check if we're naming a nearby,
                                // nameless non-baby

                                LiveObject *closestOther = 
                                    getClosestOtherPlayer( nextPlayer,
                                                           babyAge, true );

                                if( closestOther != NULL ) {
                                    
                                    if( closestOther->isEve ) {
                                        
                                        name = isEveNamingSay( m.saidText );
                                        
                                        if( name != NULL && 
                                            strcmp( name, "" ) != 0 ) {
                                            
                                            nameEve( closestOther, name );
                                            playerIndicesToSendNamesAbout.
                                                push_back( 
                                                    getLiveObjectIndex( 
                                                        closestOther->id ) );
                                    
                                            replaceNameInSaidPhrase( 
                                                name,
                                                &( m.saidText ),
                                                closestOther, true );
                                            
                                            if( ! isEveWindow() ) {
                                                // new family name created
                                                restockPostWindowFamilies();
                                                }
                                            }
                                        }
                                    else {
                                        // non-Eve
                                        nameBaby( 
                                            nextPlayer, closestOther,
                                            name, 
                                            &playerIndicesToSendNamesAbout );
                                        
                                        replaceNameInSaidPhrase( 
                                            name,
                                            &( m.saidText ),
                                            closestOther, false );
                                        }
                                    }
                                }

                            // also check if we're holding something writable
                            unsigned char metaData[ MAP_METADATA_LENGTH ];
                            int len = strlen( m.saidText );
                            
                            if( nextPlayer->holdingID > 0 &&
                                len < MAP_METADATA_LENGTH &&
                                getObject( 
                                    nextPlayer->holdingID )->writable &&
                                // and no metadata already on it
                                ! getMetadata( nextPlayer->holdingID, 
                                               metaData ) ) {

                                char *textToAdd = NULL;
                                

                                if( strstr( 
                                        getObject( nextPlayer->holdingID )->
                                        description,
                                        "+map" ) != NULL ) {
                                    // holding a potential map
                                    // add coordinates to where we're standing
                                    GridPos p = getPlayerPos( nextPlayer );
                                    
                                    textToAdd = autoSprintf( 
                                        "%s *map %d %d",
                                        m.saidText, p.x, p.y );
                                    
                                    if( strlen( textToAdd ) >= 
                                        MAP_METADATA_LENGTH ) {
                                        // too long once coords added
                                        // skip adding
                                        delete [] textToAdd;
                                        textToAdd = 
                                            stringDuplicate( m.saidText );
                                        }
                                    }
                                else {
                                    textToAdd = stringDuplicate( m.saidText );
                                    }

                                memset( metaData, 0, MAP_METADATA_LENGTH );
                                memcpy( metaData, textToAdd, 
                                        strlen( textToAdd ) + 1 );
                                
                                delete [] textToAdd;

                                nextPlayer->holdingID = 
                                    addMetadata( nextPlayer->holdingID,
                                                 metaData );

                                TransRecord *writingHappenTrans =
                                    getMetaTrans( 0, nextPlayer->holdingID );
                                
                                if( writingHappenTrans != NULL &&
                                    writingHappenTrans->newTarget > 0 &&
                                    getObject( writingHappenTrans->newTarget )
                                        ->written ) {
                                    // bare hands transition going from
                                    // writable to written
                                    // use this to transform object in 
                                    // hands as we write
                                    handleHoldingChange( 
                                        nextPlayer,
                                        writingHappenTrans->newTarget );
                                    playerIndicesToSendUpdatesAbout.
                                        push_back( i );
                                    }                    
                                }    
                            }
                        
                        makePlayerSay( nextPlayer, m.saidText );
                        }
                    else if( m.type == KILL ) {
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        tryToStartKill( nextPlayer, m.id );
                        }
                    else if( m.type == USE ) {
                        // send update even if action fails (to let them
                        // know that action is over)
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        // track whether this USE resulted in something
                        // new on the ground in case of placing a grave
                        int newGroundObject = -1;
                        GridPos newGroundObjectOrigin =
                            { nextPlayer->heldGraveOriginX,
                              nextPlayer->heldGraveOriginY };
                        
                        // save current value here, because it may 
                        // change below
                        int heldGravePlayerID = nextPlayer->heldGravePlayerID;
                        

                        char distanceUseAllowed = false;
                        
                        if( nextPlayer->holdingID > 0 ) {
                            
                            // holding something
                            ObjectRecord *heldObj = 
                                getObject( nextPlayer->holdingID );
                            
                            if( heldObj->useDistance > 1 ) {
                                // it's long-distance

                                GridPos targetPos = { m.x, m.y };
                                GridPos playerPos = { nextPlayer->xd,
                                                      nextPlayer->yd };
                                
                                double d = distance( targetPos,
                                                     playerPos );
                                
                                if( heldObj->useDistance >= d &&
                                    ! directLineBlocked( playerPos, 
                                                         targetPos ) ) {
                                    distanceUseAllowed = true;
                                    }
                                }
                            }
                        
                        if( isBiomeAllowedForPlayer( nextPlayer, m.x, m.y ) )
                        if( distanceUseAllowed 
                            ||
                            isGridAdjacent( m.x, m.y,
                                            nextPlayer->xd, 
                                            nextPlayer->yd ) 
                            ||
                            ( m.x == nextPlayer->xd &&
                              m.y == nextPlayer->yd ) ) {
                            
                            nextPlayer->actionAttempt = 1;
                            nextPlayer->actionTarget.x = m.x;
                            nextPlayer->actionTarget.y = m.y;
                            
                            if( m.x > nextPlayer->xd ) {
                                nextPlayer->facingOverride = 1;
                                }
                            else if( m.x < nextPlayer->xd ) {
                                nextPlayer->facingOverride = -1;
                                }

                            // can only use on targets next to us for now,
                            // no diags
                            
                            int target = getMapObject( m.x, m.y );
                            
                            int oldHolding = nextPlayer->holdingID;
                            
                            char accessBlocked =
                                isAccessBlocked( nextPlayer, m.x, m.y, target );
                            
                            if( accessBlocked ) {
                                // ignore action from wrong side
                                // or that players don't own
                                }
                            else if( target != 0 ) {
                                ObjectRecord *targetObj = getObject( target );
                                
                                // see if target object is permanent
                                // and has writing on it.
                                // if so, read by touching it
                                
                                if( targetObj->permanent &&
                                    targetObj->written ) {
                                    forcePlayerToRead( nextPlayer, target );
                                    }
                                

                                // try using object on this target 
                                
                                TransRecord *r = NULL;
                                char defaultTrans = false;
                                
                                char heldCanBeUsed = false;
                                char containmentTransfer = false;
                                if( // if what we're holding contains
                                    // stuff, block it from being
                                    // used as a tool
                                    nextPlayer->numContained == 0 ) {
                                    heldCanBeUsed = true;
                                    }
                                else if( nextPlayer->holdingID > 0 ) {
                                    // see if result of trans
                                    // would preserve containment

                                    r = getPTrans( nextPlayer->holdingID,
                                                  target );


                                    ObjectRecord *heldObj = getObject( 
                                        nextPlayer->holdingID );
                                    
                                    if( r != NULL && r->newActor == 0 &&
                                        r->newTarget > 0 ) {
                                        ObjectRecord *newTargetObj =
                                            getObject( r->newTarget );
                                        
                                        if( targetObj->numSlots == 0
                                            && newTargetObj->numSlots >=
                                            heldObj->numSlots
                                            &&
                                            newTargetObj->slotSize >=
                                            heldObj->slotSize ) {
                                            
                                            containmentTransfer = true;
                                            heldCanBeUsed = true;
                                            }
                                        }

                                    if( r == NULL ) {
                                        // no transition applies for this
                                        // held, whether full or empty
                                        
                                        // let it be used anyway, so
                                        // that generic transition (below)
                                        // might apply
                                        heldCanBeUsed = true;
                                        }
                                    
                                    r = NULL;
                                    }
                                

                                if( nextPlayer->holdingID >= 0 &&
                                    heldCanBeUsed ) {
                                    // negative holding is ID of baby
                                    // which can't be used
                                    // (and no bare hand action available)
                                    r = getPTrans( nextPlayer->holdingID,
                                                  target );
                                    }

                                char blockedTool = false;
                                
                                if( nextPlayer->holdingID > 0 &&
                                    r != NULL &&
                                    r->newActor != 0 ) {
                                    // make sure player can use this tool
                                    // only counts as a real use if something
                                    // is left in the hand
                                    // otherwise, it could be a stacking action
                                    // (like putting a wool pad in a bowl)

                                    // also, watch out for action where
                                    // we're inserting an object into
                                    // a container that it can also be used
                                    // on
                                    char insertion = false;
                                    ObjectRecord *heldO = 
                                        getObject( nextPlayer->holdingID );
                                    
                                    if( targetObj->numSlots > 0 &&
                                        heldO->containable &&
                                        targetObj->slotSize >=
                                        heldO->containSize &&
                                        getNumContained( m.x, m.y ) > 0 ) {
                                        
                                        insertion = true;
                                        }

                                    // also watch out for failed
                                    // tool use due to hungry work
                                    char hungBlocked = false;
                                    if( ! insertion && 
                                        r->newTarget > 0 ) {
                                        
                                        int hCost = 0;
                                        hungBlocked = isHungryWorkBlocked( 
                                            nextPlayer,
                                            r->newTarget,
                                            &hCost );
                                        }
                                    

                                    if( ! insertion &&
                                        ! hungBlocked &&
                                        ! canPlayerUseOrLearnTool( 
                                            nextPlayer,
                                            nextPlayer->holdingID ) ) {
                                        r = NULL;
                                        blockedTool = true;
                                        }
                                    }
                                
                                if( ! blockedTool &&
                                    target > 0 &&
                                    r != NULL ) {

                                    char couldBeTool = false;
                                    
                                    if( getObject( target )->permanent ) {
                                        couldBeTool = true;
                                        }
                                    else {
                                        // non-perm

                                        // some tools sit loose on ground
                                        // and then we do something to them
                                        // to make them permanent
                                        // (like pounding stakes)
                                        // Check if this is the case
                                        
                                        if( r->newTarget > 0 &&
                                            getObject( r->newTarget )->
                                            permanent ) {
                                            couldBeTool = true;
                                            }
                                        }

                                    // make sure player can use this ground-tool
                                    if( couldBeTool &&
                                        ! canPlayerUseOrLearnTool( 
                                            nextPlayer,
                                            target ) ) {
                                        r = NULL;
                                        blockedTool = true;
                                        }
                                    }
                                
                                
                                if( r != NULL &&
                                    targetObj->numSlots > 0 ) {
                                    // target has number of slots
                                    
                                    int numContained = 
                                        getNumContained( m.x, m.y );
                                    
                                    int numSlotsInNew = 0;
                                    
                                    if( r->newTarget > 0 ) {
                                        numSlotsInNew =
                                            getObject( r->newTarget )->numSlots;
                                        }
                                    
                                    if( numContained > numSlotsInNew &&
                                        numSlotsInNew == 0 ) {
                                        // not enough room in new target

                                        // check if new actor will contain
                                        // them (reverse containment transfer)
                                        
                                        if( r->newActor > 0 &&
                                            nextPlayer->numContained == 0 ) {
                                            // old actor empty
                                            
                                            int numSlotsNewActor =
                                                getObject( r->newActor )->
                                                numSlots;
                                         
                                            numSlotsInNew = numSlotsNewActor;
                                            }
                                        }


                                    if( numContained > numSlotsInNew ) {
                                        // would result in shrinking
                                        // and flinging some contained
                                        // objects
                                        // block it.
                                        heldCanBeUsed = false;
                                        r = NULL;
                                        }
                                    }
                                

                                if( r == NULL && 
                                    ( nextPlayer->holdingID != 0 || 
                                      targetObj->permanent ) &&
                                    ( isGridAdjacent( m.x, m.y,
                                                      nextPlayer->xd, 
                                                      nextPlayer->yd ) 
                                      ||
                                      ( m.x == nextPlayer->xd &&
                                        m.y == nextPlayer->yd ) ) ) {
                                    
                                    // block default transitions from
                                    // happening at a distance

                                    // search for default 
                                    r = getPTrans( -2, target );
                                        
                                    if( r != NULL ) {
                                        defaultTrans = true;
                                        }
                                    else if( nextPlayer->holdingID <= 0 || 
                                             targetObj->numSlots == 0 ) {
                                        // also consider bare-hand
                                        // action that produces
                                        // no new held item
                                        
                                        // but only on non-container
                                        // objects (example:  we don't
                                        // want to kick minecart into
                                        // motion every time we try
                                        // to add something to it)
                                        
                                        // treat this the same as
                                        // default
                                        r = getPTrans( 0, target );
                                        
                                        if( r != NULL && 
                                            r->newActor == 0 ) {
                                            
                                            defaultTrans = true;
                                            }
                                        else {
                                            r = NULL;
                                            }
                                        }
                                    }
                                

                                if( r != NULL &&
                                    r->newTarget > 0 &&
                                    r->newTarget != target ) {
                                    
                                    ObjectRecord *newTargetObj = 
                                        getObject( r->newTarget );
                                    
                                    // target would change here
                                    if( getMapFloor( m.x, m.y ) != 0 ) {
                                        // floor present
                                        
                                        // make sure new target allowed 
                                        // to exist on floor
                                        if( strstr( newTargetObj->
                                                    description, 
                                                    "groundOnly" ) != NULL ) {
                                            r = NULL;
                                            }
                                        }
                                    if( newTargetObj->isBiomeLimited &&
                                        ! canBuildInBiome( 
                                            newTargetObj,
                                            getMapBiome( m.x,
                                                         m.y ) ) ) {
                                        // can't make this object
                                        // in this biome
                                        r = NULL;
                                        }
                                    }
                                

                                if( r == NULL && 
                                    nextPlayer->holdingID > 0 &&
                                    ! blockedTool ) {
                                    
                                    logTransitionFailure( 
                                        nextPlayer->holdingID,
                                        target );
                                    }
                                
                                double playerAge = computeAge( nextPlayer );
                                
                                int hungryWorkCost = 0;
                                
                                if( r != NULL && 
                                    r->newTarget > 0 ) {

                                    if( isHungryWorkBlocked( 
                                            nextPlayer,
                                            r->newTarget,
                                            &hungryWorkCost ) ) {
                                        r = NULL;
                                        }
                                    }


                                if( r != NULL && containmentTransfer ) {
                                    // special case contained items
                                    // moving from actor into new target
                                    // (and hand left empty)
                                    setResponsiblePlayer( - nextPlayer->id );
                                    
                                    setMapObject( m.x, m.y, r->newTarget );
                                    newGroundObject = r->newTarget;
                                    
                                    setResponsiblePlayer( -1 );
                                    
                                    transferHeldContainedToMap( nextPlayer,
                                                                m.x, m.y );
                                    handleHoldingChange( nextPlayer,
                                                         r->newActor );

                                    setHeldGraveOrigin( nextPlayer, m.x, m.y,
                                                        r->newTarget );
                                    }
                                else if( r != NULL &&
                                    // are we old enough to handle
                                    // what we'd get out of this transition?
                                    ( ( r->newActor == 0 &&
                                        playerAge >= defaultActionAge )
                                      || 
                                      ( r->newActor > 0 &&
                                        canPickup( r->newActor, playerAge ) ) ) 
                                    &&
                                    // does this create a blocking object?
                                    // only consider vertical-blocking
                                    // objects (like vertical walls and doors)
                                    // because these look especially weird
                                    // on top of a player
                                    // We can detect these because they 
                                    // also hug the floor
                                    // Horizontal doors look fine when
                                    // closed on player because of their
                                    // vertical offset.
                                    //
                                    // if so, make sure there's not someone
                                    // standing still there
                                    ( r->newTarget == 0 ||
                                      ! 
                                      ( getObject( r->newTarget )->
                                          blocksWalking
                                        &&
                                        getObject( r->newTarget )->
                                          floorHugging )
                                      ||
                                      isMapSpotEmptyOfPlayers( m.x, 
                                                               m.y ) ) ) {
                                    
                                    if( ! defaultTrans ) {    
                                        handleHoldingChange( nextPlayer,
                                                             r->newActor );
                                        
                                        setHeldGraveOrigin( nextPlayer, 
                                                            m.x, m.y,
                                                            r->newTarget );
                                        
                                        if( r->target > 0 ) {    
                                            nextPlayer->heldTransitionSourceID =
                                                r->target;
                                            }
                                        else {
                                            nextPlayer->heldTransitionSourceID =
                                                -1;
                                            }
                                        }
                                    


                                    // has target shrunken as a container?
                                    int oldSlots = 
                                        getNumContainerSlots( target );
                                    int newSlots = 
                                        getNumContainerSlots( r->newTarget );
                                    
                                    if( oldSlots > 0 &&
                                        newSlots == 0 && 
                                        r->actor == 0 &&
                                        r->newActor > 0
                                        &&
                                        getNumContainerSlots( r->newActor ) ==
                                        oldSlots &&
                                        getObject( r->newActor )->slotSize >=
                                        targetObj->slotSize ) {
                                        
                                        // bare-hand action that results
                                        // in something new in hand
                                        // with same number of slots 
                                        // as target
                                        // keep what was contained

                                        FullMapContained f =
                                            getFullMapContained( m.x, m.y );

                                        setContained( nextPlayer, f );
                                    
                                        clearAllContained( m.x, m.y );
                                        
                                        restretchDecays( 
                                            nextPlayer->numContained,
                                            nextPlayer->containedEtaDecays,
                                            target, r->newActor );
                                        }
                                    else {
                                        // target on ground changed
                                        // and we don't have the same
                                        // number of slots in a new held obj
                                        
                                        shrinkContainer( m.x, m.y, newSlots );
                                    
                                        if( newSlots > 0 ) {    
                                            restretchMapContainedDecays( 
                                                m.x, m.y,
                                                target,
                                                r->newTarget );
                                            }
                                        }
                                    
                                    
                                    timeSec_t oldEtaDecay = 
                                        getEtaDecay( m.x, m.y );
                                    
                                    setResponsiblePlayer( - nextPlayer->id );
                                    
                                    if( r->newTarget > 0 
                                        && getObject( r->newTarget )->floor ) {

                                        // it turns into a floor
                                        setMapObject( m.x, m.y, 0 );
                                        
                                        setMapFloor( m.x, m.y, r->newTarget );
                                        
                                        if( r->newTarget == target ) {
                                            // unchanged
                                            // keep old decay in place
                                            setFloorEtaDecay( m.x, m.y, 
                                                              oldEtaDecay );
                                            }
                                        }
                                    else {    
                                        setMapObject( m.x, m.y, r->newTarget );
                                        newGroundObject = r->newTarget;
                                        }
                                    
                                    if( hungryWorkCost > 0 ) {
                                        if( nextPlayer->yummyBonusStore > 0 ) {
                                            if( nextPlayer->yummyBonusStore
                                                >= hungryWorkCost ) {
                                                nextPlayer->yummyBonusStore -=
                                                    hungryWorkCost;
                                                hungryWorkCost = 0;
                                                }
                                            else {
                                                hungryWorkCost -= 
                                                    nextPlayer->yummyBonusStore;
                                                nextPlayer->yummyBonusStore = 0;
                                                }
                                            }
                                        
                                        nextPlayer->foodStore -= hungryWorkCost;
                                        
                                        // we checked above, so player
                                        // never is taken down below 5 here
                                        nextPlayer->foodUpdate = true;
                                        }
                                    
                                    
                                    setResponsiblePlayer( -1 );

                                    if( target == r->newTarget ) {
                                        // target not changed
                                        // keep old decay in place
                                        setEtaDecay( m.x, m.y, oldEtaDecay );
                                        }
                                    
                                    if( target > 0 && r->newTarget > 0 &&
                                        target != r->newTarget &&
                                        ! getObject( target )->isOwned &&
                                        getObject( r->newTarget )->isOwned ) {
                                        
                                        // player just created an owned
                                        // object here
                                        GridPos newPos = { m.x, m.y };

                                        nextPlayer->
                                            ownedPositions.push_back( newPos );
                                        newOwnerPos.push_back( newPos );
                                        }
                                

                                    if( r->actor == 0 &&
                                        target > 0 && r->newTarget > 0 &&
                                        target != r->newTarget ) {
                                        
                                        TransRecord *oldDecayTrans = 
                                            getTrans( -1, target );
                                        
                                        TransRecord *newDecayTrans = 
                                            getTrans( -1, r->newTarget );
                                        
                                        if( oldDecayTrans != NULL &&
                                            newDecayTrans != NULL  &&
                                            oldDecayTrans->epochAutoDecay ==
                                            newDecayTrans->epochAutoDecay &&
                                            oldDecayTrans->autoDecaySeconds ==
                                            newDecayTrans->autoDecaySeconds &&
                                            oldDecayTrans->autoDecaySeconds 
                                            > 0 ) {
                                            
                                            // old target and new
                                            // target decay into something
                                            // in same amount of time
                                            // and this was a bare-hand
                                            // action
                                            
                                            // doesn't matter if they 
                                            // decay into SAME thing.

                                            // keep old decay time in place
                                            // (instead of resetting timer)
                                            setEtaDecay( m.x, m.y, 
                                                         oldEtaDecay );
                                            }
                                        }
                                    

                                    

                                    if( r->newTarget != 0 ) {
                                        
                                        handleMapChangeToPaths( 
                                            m.x, m.y,
                                            getObject( r->newTarget ),
                                            &playerIndicesToSendUpdatesAbout );
                                        }
                                    }
                                else if( nextPlayer->holdingID == 0 &&
                                         ! targetObj->permanent &&
                                         canPickup( targetObj->id,
                                                    computeAge( 
                                                        nextPlayer ) ) ) {
                                    // no bare-hand transition applies to
                                    // this non-permanent target object
                                    
                                    // treat it like pick up
                                    
                                    pickupToHold( nextPlayer, m.x, m.y,
                                                  target );
                                    }
                                else if( nextPlayer->holdingID == 0 &&
                                         targetObj->permanent ) {
                                    
                                    // try removing from permanent
                                    // container
                                    removeFromContainerToHold( nextPlayer,
                                                               m.x, m.y,
                                                               m.i );
                                    }         
                                else if( nextPlayer->holdingID > 0 ) {
                                    // try adding what we're holding to
                                    // target container
                                    
                                    addHeldToContainer(
                                        nextPlayer, target, m.x, m.y );
                                    }
                                

                                if( targetObj->permanent &&
                                    targetObj->foodValue > 0 ) {
                                    
                                    // just touching this object
                                    // causes us to eat from it
                                    
                                    nextPlayer->justAte = true;
                                    nextPlayer->justAteID = 
                                        targetObj->id;

                                    nextPlayer->lastAteID = 
                                        targetObj->id;
                                    nextPlayer->lastAteFillMax =
                                        nextPlayer->foodStore;
                                    
                                    nextPlayer->foodStore += 
                                        lrint( foodScaleFactor *
                                               targetObj->foodValue );
                                    
                                    updateYum( nextPlayer, targetObj->id );
                                    

                                    logEating( targetObj->id,
                                               targetObj->foodValue + eatBonus,
                                               computeAge( nextPlayer ),
                                               m.x, m.y );
                                    
                                    nextPlayer->foodStore += eatBonus;

                                    int cap = 
                                        nextPlayer->lastReportedFoodCapacity;
                                    
                                    if( nextPlayer->foodStore > cap ) {
    
                                        int over = nextPlayer->foodStore - cap;
                                        
                                        nextPlayer->foodStore = cap;

                                        int overflowCap = 
                                            computeOverflowFoodCapacity( cap );

                                        if( over > overflowCap ) {
                                            over = overflowCap;
                                            }
                                        nextPlayer->yummyBonusStore += over;
                                        }

                                    
                                    // we eat everything BUT what
                                    // we picked from it, if anything
                                    if( oldHolding == 0 && 
                                        nextPlayer->holdingID != 0 ) {
                                        
                                        ObjectRecord *newHeld =
                                            getObject( nextPlayer->holdingID );
                                        
                                        if( newHeld->foodValue > 0 ) {
                                            nextPlayer->foodStore -=
                                                newHeld->foodValue;

                                            if( nextPlayer->lastAteFillMax >
                                                nextPlayer->foodStore ) {
                                                
                                                nextPlayer->foodStore =
                                                    nextPlayer->lastAteFillMax;
                                                }
                                            }
                                        
                                        }
                                    
                                    
                                    if( targetObj->alcohol != 0 ) {
                                        drinkAlcohol( nextPlayer,
                                                      targetObj->alcohol );
                                        }


                                    nextPlayer->foodDecrementETASeconds =
                                        Time::getCurrentTime() +
                                        computeFoodDecrementTimeSeconds( 
                                            nextPlayer );
                                    
                                    nextPlayer->foodUpdate = true;
                                    }
                                }
                            else if( nextPlayer->holdingID > 0 ) {
                                // target location emtpy
                                // target not where we're standing
                                // we're holding something
                                
                                char usedOnFloor = false;
                                int floorID = getMapFloor( m.x, m.y );
                                
                                if( floorID > 0 ) {
                                    
                                    TransRecord *r = 
                                        getPTrans( nextPlayer->holdingID,
                                                  floorID );
                                
                                    char blockedTool = false;
                                    
                                    if( nextPlayer->holdingID > 0 &&
                                        r != NULL ) {
                                        // make sure player can use this tool
                                    
                                        if( ! canPlayerUseOrLearnTool( 
                                                nextPlayer,
                                                nextPlayer->holdingID ) ) {
                                            r = NULL;
                                            blockedTool = true;
                                            }
                                        }
                                    
                                    // floor might be a tool too
                                    if( r != NULL ) {
                                        if( ! canPlayerUseOrLearnTool( 
                                                nextPlayer, floorID ) ) {
                                            r = NULL;
                                            blockedTool = true;
                                            }
                                        }


                                    if( r == NULL && ! blockedTool ) {
                                        logTransitionFailure( 
                                            nextPlayer->holdingID,
                                            floorID );
                                        }
                                        

                                    if( r != NULL && 
                                        // make sure we're not too young
                                        // to hold result of on-floor
                                        // transition
                                        ( r->newActor == 0 ||
                                          canPickup( 
                                              r->newActor,
                                              computeAge( nextPlayer ) ) ) ) {
                                        
                                        // applies to floor
                                        int resultID = r->newTarget;
                                        
                                        if( getObject( resultID )->floor ) {
                                            // changing floor to floor
                                            // go ahead
                                            usedOnFloor = true;
                                            
                                            if( resultID != floorID ) {
                                                setMapFloor( m.x, m.y,
                                                             resultID );
                                                }
                                            handleHoldingChange( nextPlayer,
                                                                 r->newActor );
                                            
                                            setHeldGraveOrigin( nextPlayer, 
                                                                m.x, m.y,
                                                                resultID );
                                            }
                                        else {
                                            // changing floor to non-floor
                                            char canPlace = true;
                                            if( getObject( resultID )->
                                                blocksWalking &&
                                                ! isMapSpotEmpty( m.x, m.y ) ) {
                                                canPlace = false;
                                                }
                                            
                                            if( canPlace ) {
                                                setMapFloor( m.x, m.y, 0 );
                                                
                                                setMapObject( m.x, m.y,
                                                              resultID );
                                                
                                                handleHoldingChange( 
                                                    nextPlayer,
                                                    r->newActor );
                                                setHeldGraveOrigin( nextPlayer, 
                                                                    m.x, m.y,
                                                                    resultID );
                                            
                                                usedOnFloor = true;
                                                }
                                            }
                                        }
                                    }
                                


                                // consider a use-on-bare-ground transtion
                                
                                ObjectRecord *obj = 
                                    getObject( nextPlayer->holdingID );
                                
                                if( ! usedOnFloor && obj->foodValue == 0 &&
                                    // player didn't try to click something
                                    m.id == -1 ) {
                                    
                                    // get no-target transtion
                                    // (not a food transition, since food
                                    //   value is 0)
                                    TransRecord *r = 
                                        getPTrans( nextPlayer->holdingID, 
                                                  -1 );


                                    char canPlace = false;
                                    
                                    if( r != NULL &&
                                        r->newTarget != 0 
                                        && 
                                        // make sure we're not too young
                                        // to hold result of bare ground
                                        // transition
                                        ( r->newActor == 0 ||
                                          canPickup( 
                                              r->newActor,
                                              computeAge( nextPlayer ) ) ) ) {
                                        
                                        canPlace = true;
                                        
                                        ObjectRecord *newTargetObj =
                                            getObject( r->newTarget );
                                        

                                        if( newTargetObj->blocksWalking &&
                                            ! isMapSpotEmpty( m.x, m.y ) ) {
                                            
                                            // can't do on-bare ground
                                            // transition where person 
                                            // standing
                                            // if it creates a blocking 
                                            // object
                                            canPlace = false;
                                            }
                                        else if( 
                                            strstr( newTargetObj->description, 
                                                    "groundOnly" ) != NULL
                                            &&
                                            getMapFloor( m.x, m.y ) != 0 ) {
                                            // floor present
                                        
                                            // new target not allowed 
                                            // to exist on floor
                                            canPlace = false;
                                            }
                                        else if( newTargetObj->isBiomeLimited &&
                                                 ! canBuildInBiome( 
                                                     newTargetObj,
                                                     getMapBiome( m.x,
                                                                  m.y ) ) ) {
                                            // can't make this object
                                            // in this biome
                                            canPlace = false;
                                            }
                                        }
                                    
                                    if( canPlace ) {
                                        nextPlayer->heldTransitionSourceID =
                                            nextPlayer->holdingID;
                                        
                                        if( nextPlayer->numContained > 0 &&
                                            r->newActor == 0 &&
                                            r->newTarget > 0 &&
                                            getObject( r->newTarget )->numSlots 
                                            >= nextPlayer->numContained &&
                                            getObject( r->newTarget )->slotSize
                                            >= obj->slotSize ) {

                                            // use on bare ground with full
                                            // container that leaves
                                            // hand empty
                                            
                                            // and there's room in newTarget

                                            setResponsiblePlayer( 
                                                - nextPlayer->id );

                                            setMapObject( m.x, m.y, 
                                                          r->newTarget );
                                            newGroundObject = r->newTarget;

                                            setResponsiblePlayer( -1 );
                                    
                                            transferHeldContainedToMap( 
                                                nextPlayer, m.x, m.y );
                                            
                                            handleHoldingChange( nextPlayer,
                                                                 r->newActor );
                                            
                                            setHeldGraveOrigin( nextPlayer, 
                                                                m.x, m.y,
                                                                r->newTarget );
                                            }
                                        else {
                                            handleHoldingChange( nextPlayer,
                                                                 r->newActor );
                                            
                                            setHeldGraveOrigin( nextPlayer, 
                                                                m.x, m.y,
                                                                r->newTarget );
                                            
                                            setResponsiblePlayer( 
                                                - nextPlayer->id );
                                            
                                            if( r->newTarget > 0 
                                                && getObject( r->newTarget )->
                                                floor ) {
                                                
                                                setMapFloor( m.x, m.y, 
                                                             r->newTarget );
                                                }
                                            else {    
                                                setMapObject( m.x, m.y, 
                                                              r->newTarget );
                                                newGroundObject = r->newTarget;
                                                }
                                            
                                            setResponsiblePlayer( -1 );
                                            
                                            handleMapChangeToPaths( 
                                             m.x, m.y,
                                             getObject( r->newTarget ),
                                             &playerIndicesToSendUpdatesAbout );
                                            }
                                        }
                                    }
                                }
                            

                            if( target == 0 && newGroundObject > 0 ) {
                                // target location was empty, and now it's not
                                // check if we moved a grave here
                            
                                ObjectRecord *o = getObject( newGroundObject );
                                
                                if( strstr( o->description, "origGrave" ) 
                                    != NULL ) {
                                    
                                    setGravePlayerID( 
                                        m.x, m.y, heldGravePlayerID );
                                    
                                    int swapDest = 
                                        isGraveSwapDest( m.x, m.y, 
                                                         nextPlayer->id );

                                    GraveMoveInfo g = { 
                                        { newGroundObjectOrigin.x,
                                          newGroundObjectOrigin.y },
                                        { m.x,
                                          m.y }, 
                                        swapDest };
                                    newGraveMoves.push_back( g );
                                    }
                                }
                            }
                        }
                    else if( m.type == BABY ) {
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        if( computeAge( nextPlayer ) >= minPickupBabyAge 
                            &&
                            ( isGridAdjacent( m.x, m.y,
                                              nextPlayer->xd, 
                                              nextPlayer->yd ) 
                              ||
                              ( m.x == nextPlayer->xd &&
                                m.y == nextPlayer->yd ) ) ) {
                            
                            nextPlayer->actionAttempt = 1;
                            nextPlayer->actionTarget.x = m.x;
                            nextPlayer->actionTarget.y = m.y;
                            
                            if( m.x > nextPlayer->xd ) {
                                nextPlayer->facingOverride = 1;
                                }
                            else if( m.x < nextPlayer->xd ) {
                                nextPlayer->facingOverride = -1;
                                }


                            if( nextPlayer->holdingID == 0 ) {
                                // target location empty and 
                                // and our hands are empty
                                
                                // check if there's a baby to pick up there

                                // is anyone there?
                                LiveObject *hitPlayer = 
                                    getHitPlayer( m.x, m.y, m.id, 
                                                  false, babyAge );
                                
                                if( hitPlayer != NULL &&
                                    !hitPlayer->heldByOther &&
                                    computeAge( hitPlayer ) < babyAge  ) {
                                    
                                    // negative holding IDs to indicate
                                    // holding another player
                                    nextPlayer->holdingID = -hitPlayer->id;
                                    holdingSomethingNew( nextPlayer );
                                    
                                    nextPlayer->holdingEtaDecay = 0;

                                    hitPlayer->heldByOther = true;
                                    hitPlayer->heldByOtherID = nextPlayer->id;
                                    
                                    if( hitPlayer->heldByOtherID ==
                                        hitPlayer->parentID ) {
                                        hitPlayer->everHeldByParent = true;
                                        }
                                    

                                    // force baby to drop what they are
                                    // holding

                                    if( hitPlayer->holdingID != 0 ) {
                                        // never drop held wounds
                                        // they are the only thing a baby can
                                        // while held
                                        if( ! hitPlayer->holdingWound &&
                                            ! hitPlayer->holdingBiomeSickness &&
                                            hitPlayer->holdingID > 0 ) {
                                            handleDrop( 
                                                m.x, m.y, hitPlayer,
                                             &playerIndicesToSendUpdatesAbout );
                                            }
                                        }
                                    
                                    if( hitPlayer->xd != hitPlayer->xs
                                        ||
                                        hitPlayer->yd != hitPlayer->ys ) {
                                        
                                        // force baby to stop moving
                                        hitPlayer->xd = m.x;
                                        hitPlayer->yd = m.y;
                                        hitPlayer->xs = m.x;
                                        hitPlayer->ys = m.y;
                                        
                                        // but don't send an update
                                        // about this
                                        // (everyone will get the pick-up
                                        //  update for the holding adult)
                                        }
                                    
                                    // if adult fertile female, baby auto-fed
                                    if( isFertileAge( nextPlayer ) ) {
                                        
                                        hitPlayer->foodStore = 
                                            computeFoodCapacity( hitPlayer );
                
                                        hitPlayer->foodUpdate = true;
                                        hitPlayer->responsiblePlayerID =
                                            nextPlayer->id;
                                        
                                        // reset their food decrement time
                                        hitPlayer->foodDecrementETASeconds =
                                            Time::getCurrentTime() +
                                            computeFoodDecrementTimeSeconds( 
                                                hitPlayer );

                                        // fixed cost to pick up baby
                                        // this still encourages baby-parent
                                        // communication so as not
                                        // to get the most mileage out of 
                                        // food
                                        int nurseCost = 1;
                                        
                                        if( nextPlayer->yummyBonusStore > 0 ) {
                                            nextPlayer->yummyBonusStore -= 
                                                nurseCost;
                                            nurseCost = 0;
                                            if( nextPlayer->yummyBonusStore < 
                                                0 ) {
                                                
                                                // not enough to cover full 
                                                // nurse cost

                                                // pass remaining nurse
                                                // cost onto main food store
                                                nurseCost = - nextPlayer->
                                                    yummyBonusStore;
                                                nextPlayer->yummyBonusStore = 0;
                                                }
                                            }
                                        

                                        nextPlayer->foodStore -= nurseCost;
                                        
                                        if( nextPlayer->foodStore < 0 ) {
                                            // catch mother death later
                                            // at her next food decrement
                                            nextPlayer->foodStore = 0;
                                            }
                                        // leave their food decrement
                                        // time alone
                                        nextPlayer->foodUpdate = true;
                                        }
                                    
                                    nextPlayer->heldOriginValid = 1;
                                    nextPlayer->heldOriginX = m.x;
                                    nextPlayer->heldOriginY = m.y;
                                    nextPlayer->heldTransitionSourceID = -1;
                                    }
                                
                                }
                            }
                        }
                    else if( m.type == SELF || m.type == UBABY ) {
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        char holdingFood = false;
                        char holdingDrugs = false;
                        
                        if( nextPlayer->holdingID > 0 ) {
                            ObjectRecord *obj = 
                                getObject( nextPlayer->holdingID );
                            
                            if( obj->foodValue > 0 ) {
                                holdingFood = true;

                                if( strstr( obj->description, "remapStart" )
                                    != NULL ) {
                                    // don't count drugs as food to 
                                    // feed other people
                                    holdingFood = false;
                                    holdingDrugs = true;
                                    }
                                }
                            }
                        
                        LiveObject *targetPlayer = NULL;
                        
                        if( nextPlayer->holdingID < 0 ) {
                            // holding a baby
                            // don't allow this action through
                            // keep targetPlayer NULL
                            }
                        else if( m.type == SELF ) {
                            if( m.x == nextPlayer->xd &&
                                m.y == nextPlayer->yd ) {
                                
                                // use on self
                                targetPlayer = nextPlayer;
                                }
                            }
                        else if( m.type == UBABY ) {
                            
                            if( isGridAdjacent( m.x, m.y,
                                                nextPlayer->xd, 
                                                nextPlayer->yd ) ||
                                ( m.x == nextPlayer->xd &&
                                  m.y == nextPlayer->yd ) ) {
                                

                                if( m.x > nextPlayer->xd ) {
                                    nextPlayer->facingOverride = 1;
                                    }
                                else if( m.x < nextPlayer->xd ) {
                                    nextPlayer->facingOverride = -1;
                                    }
                                
                                // try click on baby
                                int hitIndex;
                                LiveObject *hitPlayer = 
                                    getHitPlayer( m.x, m.y, m.id,
                                                  false, 
                                                  babyAge, -1, &hitIndex );
                                
                                if( hitPlayer != NULL && holdingDrugs ) {
                                    // can't even feed baby drugs
                                    // too confusing
                                    hitPlayer = NULL;
                                    }

                                if( hitPlayer == NULL ||
                                    hitPlayer == nextPlayer ) {
                                    // try click on elderly
                                    hitPlayer = 
                                        getHitPlayer( m.x, m.y, m.id,
                                                      false, -1, 
                                                      55, &hitIndex );
                                    }
                                
                                if( ( hitPlayer == NULL ||
                                      hitPlayer == nextPlayer )
                                    &&
                                    holdingFood ) {
                                    
                                    // feeding action 
                                    // try click on everyone
                                    hitPlayer = 
                                        getHitPlayer( m.x, m.y, m.id,
                                                      false, -1, -1, 
                                                      &hitIndex );
                                    }
                                
                                
                                if( ( hitPlayer == NULL ||
                                      hitPlayer == nextPlayer )
                                    &&
                                    ! holdingDrugs ) {
                                    
                                    // see if clicked-on player is dying
                                    hitPlayer = 
                                        getHitPlayer( m.x, m.y, m.id,
                                                      false, -1, -1, 
                                                      &hitIndex );
                                    if( hitPlayer != NULL &&
                                        ! hitPlayer->dying ) {
                                        hitPlayer = NULL;
                                        }
                                    }
                                

                                if( hitPlayer != NULL &&
                                    hitPlayer != nextPlayer ) {
                                    
                                    targetPlayer = hitPlayer;
                                    
                                    playerIndicesToSendUpdatesAbout.push_back( 
                                        hitIndex );
                                    
                                    targetPlayer->responsiblePlayerID =
                                            nextPlayer->id;
                                    }
                                }
                            }
                        

                        if( targetPlayer != NULL ) {
                            
                            // use on self/baby
                            nextPlayer->actionAttempt = 1;
                            nextPlayer->actionTarget.x = m.x;
                            nextPlayer->actionTarget.y = m.y;
                            

                            if( targetPlayer != nextPlayer &&
                                targetPlayer->dying &&
                                ! holdingFood ) {
                                
                                // try healing wound
                                    
                                TransRecord *healTrans =
                                    getMetaTrans( nextPlayer->holdingID,
                                                  targetPlayer->holdingID );
                                
                                char oldEnough = true;

                                if( healTrans != NULL ) {
                                    int healerWillHold = healTrans->newActor;
                                    
                                    if( healerWillHold > 0 ) {
                                        if( ! canPickup( 
                                                healerWillHold,
                                                computeAge( nextPlayer ) ) ) {
                                            oldEnough = false;
                                            }
                                        }
                                    }
                                

                                if( oldEnough && healTrans != NULL ) {
                                    targetPlayer->holdingID =
                                        healTrans->newTarget;
                                    holdingSomethingNew( targetPlayer );
                                    
                                    // their wound has been changed
                                    // no longer track embedded weapon
                                    targetPlayer->embeddedWeaponID = 0;
                                    targetPlayer->embeddedWeaponEtaDecay = 0;
                                    
                                    
                                    nextPlayer->holdingID = 
                                        healTrans->newActor;
                                    holdingSomethingNew( nextPlayer );
                                    
                                    setFreshEtaDecayForHeld( 
                                        nextPlayer );
                                    setFreshEtaDecayForHeld( 
                                        targetPlayer );
                                    
                                    nextPlayer->heldOriginValid = 0;
                                    nextPlayer->heldOriginX = 0;
                                    nextPlayer->heldOriginY = 0;
                                    nextPlayer->heldTransitionSourceID = 
                                        healTrans->target;
                                    
                                    targetPlayer->heldOriginValid = 0;
                                    targetPlayer->heldOriginX = 0;
                                    targetPlayer->heldOriginY = 0;
                                    targetPlayer->heldTransitionSourceID = 
                                        -1;
                                    
                                    if( targetPlayer->holdingID == 0 ) {
                                        // not dying anymore
                                        setNoLongerDying( 
                                            targetPlayer,
                                            &playerIndicesToSendHealingAbout );
                                        }
                                    else {
                                        // wound changed?

                                        ForcedEffects e = 
                                            checkForForcedEffects( 
                                                targetPlayer->holdingID );
                            
                                        if( e.emotIndex != -1 ) {
                                            targetPlayer->emotFrozen = true;
                                            targetPlayer->emotFrozenIndex =
                                                e.emotIndex;
                                            newEmotPlayerIDs.push_back( 
                                                targetPlayer->id );
                                            newEmotIndices.push_back( 
                                                e.emotIndex );
                                            newEmotTTLs.push_back( e.ttlSec );
                                            interruptAnyKillEmots( 
                                                targetPlayer->id, e.ttlSec );
                                            }
                                        if( e.foodCapModifier != 1 ) {
                                            targetPlayer->yummyBonusStore = 0;
                                            targetPlayer->foodCapModifier = 
                                                e.foodCapModifier;
                                            targetPlayer->foodUpdate = true;
                                            }
                                        if( e.feverSet ) {
                                            targetPlayer->fever = e.fever;
                                            }
                                        }
                                    }
                                }
                            else if( nextPlayer->holdingID > 0 ) {
                                ObjectRecord *obj = 
                                    getObject( nextPlayer->holdingID );
                                
                                // don't use "live" computed capacity here
                                // because that will allow player to spam
                                // click to pack in food between food
                                // decrements when they are growing
                                // instead, stick to the food cap shown
                                // in the client (what we last reported
                                // to them)
                                int cap = nextPlayer->lastReportedFoodCapacity;
                                

                                // first case:
                                // player clicked on clothing
                                // try adding held into clothing, but if
                                // that fails go on to other cases

                                // except do not force them to eat
                                // something that could have gone
                                // into a full clothing container!
                                char couldHaveGoneIn = false;

                                ObjectRecord *clickedClothing = NULL;
                                TransRecord *clickedClothingTrans = NULL;
                                if( m.i >= 0 &&
                                    m.i < NUM_CLOTHING_PIECES ) {
                                    clickedClothing =
                                        clothingByIndex( nextPlayer->clothing,
                                                         m.i );
                                    
                                    if( clickedClothing != NULL ) {
                                        
                                        clickedClothingTrans =
                                            getPTrans( nextPlayer->holdingID,
                                                       clickedClothing->id );
                                        
                                        if( clickedClothingTrans != NULL ) {
                                            int na =
                                                clickedClothingTrans->newActor;
                                            
                                            if( na > 0 &&
                                                ! canPickup( 
                                                    na,
                                                    computeAge( 
                                                        nextPlayer ) ) ) {
                                                // too young for trans
                                                clickedClothingTrans = NULL;
                                                }

                                            int nt = 
                                                clickedClothingTrans->newTarget;
                                            
                                            if( nt > 0 &&
                                                getObject( nt )->clothing 
                                                == 'n' ) {
                                                // don't allow transitions
                                                // that leave a non-wearable
                                                // item on your body
                                                clickedClothingTrans = NULL;
                                                }
                                            }
                                        }
                                    }
                                

                                if( targetPlayer == nextPlayer &&
                                    m.i >= 0 && 
                                    m.i < NUM_CLOTHING_PIECES &&
                                    addHeldToClothingContainer( 
                                        nextPlayer,
                                        m.i,
                                        false,
                                        &couldHaveGoneIn) ) {
                                    // worked!
                                    }
                                // next case:  can what they're holding
                                // be used to transform clothing?
                                else if( m.i >= 0 &&
                                         m.i < NUM_CLOTHING_PIECES &&
                                         clickedClothing != NULL &&
                                         clickedClothingTrans != NULL ) {
                                    
                                    // NOTE:
                                    // this is a niave way of handling
                                    // this case, and it doesn't deal
                                    // with all kinds of possible complexities
                                    // (like if the clothing decay time should
                                    //  change, or number of slots change)
                                    // Assuming that we won't add transitions
                                    // for clothing with those complexities
                                    // Works for removing sword
                                    // from backpack

                                    handleHoldingChange(
                                        nextPlayer,
                                        clickedClothingTrans->newActor );
                                    
                                    setClothingByIndex( 
                                        &( nextPlayer->clothing ), 
                                        m.i,
                                        getObject( 
                                            clickedClothingTrans->newTarget ) );
                                    }
                                // next case, holding food
                                // that couldn't be put into clicked clothing
                                else if( obj->foodValue > 0 && 
                                         targetPlayer->foodStore < cap &&
                                         ! couldHaveGoneIn ) {
                                    
                                    targetPlayer->justAte = true;
                                    targetPlayer->justAteID = 
                                        nextPlayer->holdingID;

                                    targetPlayer->lastAteID = 
                                        nextPlayer->holdingID;
                                    targetPlayer->lastAteFillMax =
                                        targetPlayer->foodStore;
                                    
                                    targetPlayer->foodStore += 
                                        lrint( foodScaleFactor * 
                                               obj->foodValue );
                                    
                                    updateYum( targetPlayer, obj->id,
                                               targetPlayer == nextPlayer );

                                    logEating( obj->id,
                                               obj->foodValue + eatBonus,
                                               computeAge( targetPlayer ),
                                               m.x, m.y );
                                    
                                    targetPlayer->foodStore += eatBonus;

                                    
                                    if( targetPlayer->foodStore > cap ) {
                                        int over = 
                                            targetPlayer->foodStore - cap;
                                        
                                        targetPlayer->foodStore = cap;

                                        int overflowCap = 
                                            computeOverflowFoodCapacity( cap );

                                        if( over > overflowCap ) {
                                            over = overflowCap;
                                            }
                                        targetPlayer->yummyBonusStore += over;
                                        }
                                    targetPlayer->foodDecrementETASeconds =
                                        Time::getCurrentTime() +
                                        computeFoodDecrementTimeSeconds( 
                                            targetPlayer );
                                    
                                    // get eat transtion
                                    TransRecord *r = 
                                        getPTrans( nextPlayer->holdingID, 
                                                  -1 );

                                    

                                    if( r != NULL ) {
                                        int oldHolding = nextPlayer->holdingID;
                                        nextPlayer->holdingID = r->newActor;
                                        holdingSomethingNew( nextPlayer,
                                                             oldHolding );

                                        if( oldHolding !=
                                            nextPlayer->holdingID ) {
                                            
                                            setFreshEtaDecayForHeld( 
                                                nextPlayer );
                                            }
                                        }
                                    else {
                                        // default, holding nothing after eating
                                        nextPlayer->holdingID = 0;
                                        nextPlayer->holdingEtaDecay = 0;
                                        }
                                    
                                    if( obj->alcohol != 0 ) {
                                        drinkAlcohol( targetPlayer,
                                                      obj->alcohol );
                                        }


                                    nextPlayer->heldOriginValid = 0;
                                    nextPlayer->heldOriginX = 0;
                                    nextPlayer->heldOriginY = 0;
                                    nextPlayer->heldTransitionSourceID = -1;
                                    
                                    targetPlayer->foodUpdate = true;
                                    }
                                // final case, holding clothing that
                                // we could put on
                                else if( obj->clothing != 'n' &&
                                         ( targetPlayer == nextPlayer
                                           || 
                                           computeAge( targetPlayer ) < 
                                           babyAge) ) {
                                    
                                    // wearable, dress self or baby
                                    
                                    nextPlayer->holdingID = 0;
                                    timeSec_t oldEtaDecay = 
                                        nextPlayer->holdingEtaDecay;
                                    
                                    nextPlayer->holdingEtaDecay = 0;
                                    
                                    nextPlayer->heldOriginValid = 0;
                                    nextPlayer->heldOriginX = 0;
                                    nextPlayer->heldOriginY = 0;
                                    nextPlayer->heldTransitionSourceID = -1;
                                    
                                    ObjectRecord *oldC = NULL;
                                    timeSec_t oldCEtaDecay = 0;
                                    int oldNumContained = 0;
                                    int *oldContainedIDs = NULL;
                                    timeSec_t *oldContainedETADecays = NULL;
                                    

                                    ObjectRecord **clothingSlot = NULL;
                                    int clothingSlotIndex;

                                    switch( obj->clothing ) {
                                        case 'h':
                                            clothingSlot = 
                                                &( targetPlayer->clothing.hat );
                                            clothingSlotIndex = 0;
                                            break;
                                        case 't':
                                            clothingSlot = 
                                              &( targetPlayer->clothing.tunic );
                                            clothingSlotIndex = 1;
                                            break;
                                        case 'b':
                                            clothingSlot = 
                                                &( targetPlayer->
                                                   clothing.bottom );
                                            clothingSlotIndex = 4;
                                            break;
                                        case 'p':
                                            clothingSlot = 
                                                &( targetPlayer->
                                                   clothing.backpack );
                                            clothingSlotIndex = 5;
                                            break;
                                        case 's':
                                            if( targetPlayer->clothing.backShoe
                                                == NULL ) {

                                                clothingSlot = 
                                                    &( targetPlayer->
                                                       clothing.backShoe );
                                                clothingSlotIndex = 3;

                                                }
                                            else if( 
                                                targetPlayer->clothing.frontShoe
                                                == NULL ) {
                                                
                                                clothingSlot = 
                                                    &( targetPlayer->
                                                       clothing.frontShoe );
                                                clothingSlotIndex = 2;
                                                }
                                            else {
                                                // replace whatever shoe
                                                // doesn't match what we're
                                                // holding
                                                
                                                if( targetPlayer->
                                                    clothing.backShoe == 
                                                    obj ) {
                                                    
                                                    clothingSlot = 
                                                        &( targetPlayer->
                                                           clothing.frontShoe );
                                                    clothingSlotIndex = 2;
                                                    }
                                                else if( targetPlayer->
                                                         clothing.frontShoe == 
                                                         obj ) {
                                                    clothingSlot = 
                                                        &( targetPlayer->
                                                           clothing.backShoe );
                                                    clothingSlotIndex = 3;
                                                    }
                                                else {
                                                    // both shoes are
                                                    // different from what
                                                    // we're holding
                                                    
                                                    // pick shoe to swap
                                                    // based on what we
                                                    // clicked on
                                                    
                                                    if( m.i == 3 ) {
                                                        clothingSlot = 
                                                        &( targetPlayer->
                                                           clothing.backShoe );
                                                        clothingSlotIndex = 3;
                                                        }
                                                    else {
                                                        // default to front
                                                        // shoe
                                                        clothingSlot = 
                                                        &( targetPlayer->
                                                           clothing.frontShoe );
                                                        clothingSlotIndex = 2;
                                                        }
                                                    }
                                                }
                                            break;
                                        }
                                    
                                    if( clothingSlot != NULL ) {
                                        
                                        oldC = *clothingSlot;
                                        int ind = clothingSlotIndex;
                                        
                                        oldCEtaDecay = 
                                            targetPlayer->clothingEtaDecay[ind];
                                        
                                        oldNumContained = 
                                            targetPlayer->
                                            clothingContained[ind].size();
                                        
                                        if( oldNumContained > 0 ) {
                                            oldContainedIDs = 
                                                targetPlayer->
                                                clothingContained[ind].
                                                getElementArray();
                                            oldContainedETADecays =
                                                targetPlayer->
                                                clothingContainedEtaDecays[ind].
                                                getElementArray();
                                            }
                                        
                                        *clothingSlot = obj;
                                        targetPlayer->clothingEtaDecay[ind] =
                                            oldEtaDecay;
                                        
                                        targetPlayer->
                                            clothingContained[ind].
                                            deleteAll();
                                        targetPlayer->
                                            clothingContainedEtaDecays[ind].
                                            deleteAll();
                                            
                                        if( nextPlayer->numContained > 0 ) {
                                            
                                            targetPlayer->clothingContained[ind]
                                                .appendArray( 
                                                    nextPlayer->containedIDs,
                                                    nextPlayer->numContained );

                                            targetPlayer->
                                                clothingContainedEtaDecays[ind]
                                                .appendArray( 
                                                    nextPlayer->
                                                    containedEtaDecays,
                                                    nextPlayer->numContained );
                                                

                                            // ignore sub-contained
                                            // because clothing can
                                            // never contain containers
                                            clearPlayerHeldContained( 
                                                nextPlayer );
                                            }
                                            
                                        
                                        if( oldC != NULL ) {
                                            nextPlayer->holdingID =
                                                oldC->id;
                                            holdingSomethingNew( nextPlayer );
                                            
                                            nextPlayer->holdingEtaDecay
                                                = oldCEtaDecay;
                                            
                                            nextPlayer->numContained =
                                                oldNumContained;
                                            
                                            freePlayerContainedArrays(
                                                nextPlayer );
                                            
                                            nextPlayer->containedIDs =
                                                oldContainedIDs;
                                            nextPlayer->containedEtaDecays =
                                                oldContainedETADecays;
                                            
                                            // empty sub-contained vectors
                                            // because clothing never
                                            // never contains containers
                                            nextPlayer->subContainedIDs
                                                = new SimpleVector<int>[
                                                    nextPlayer->numContained ];
                                            nextPlayer->subContainedEtaDecays
                                                = new SimpleVector<timeSec_t>[
                                                    nextPlayer->numContained ];
                                            }
                                        }
                                    }
                                }         
                            else {
                                // empty hand on self/baby, remove clothing

                                int clothingSlotIndex = m.i;
                                
                                ObjectRecord **clothingSlot = 
                                    getClothingSlot( targetPlayer, m.i );
                                
                                
                                TransRecord *bareHandClothingTrans =
                                    getBareHandClothingTrans( nextPlayer,
                                                              clothingSlot );
                                

                                if( targetPlayer == nextPlayer &&
                                    bareHandClothingTrans != NULL ) {
                                    
                                    // bare hand transforms clothing
                                    
                                    // this may not handle all possible cases
                                    // correctly.  A naive implementation for
                                    // now.  Works for removing sword
                                    // from backpack

                                    nextPlayer->holdingID =
                                        bareHandClothingTrans->newActor;

                                    handleHoldingChange( 
                                        nextPlayer,
                                        bareHandClothingTrans->newActor );
                                    
                                    nextPlayer->heldOriginValid = 0;
                                    nextPlayer->heldOriginX = 0;
                                    nextPlayer->heldOriginY = 0;
                                    

                                    if( bareHandClothingTrans->newTarget > 0 ) {
                                        *clothingSlot = 
                                            getObject( bareHandClothingTrans->
                                                       newTarget );
                                        }
                                    else {
                                        *clothingSlot = NULL;
                                        
                                        int ind = clothingSlotIndex;
                                        
                                        targetPlayer->clothingEtaDecay[ind] = 0;
                                        
                                        targetPlayer->clothingContained[ind].
                                            deleteAll();
                                        
                                        targetPlayer->
                                            clothingContainedEtaDecays[ind].
                                            deleteAll();
                                        }
                                    }
                                else if( clothingSlot != NULL ) {
                                    // bare hand removes clothing
                                    
                                    removeClothingToHold( nextPlayer,
                                                          targetPlayer,
                                                          clothingSlot,
                                                          clothingSlotIndex );
                                    }
                                }
                            }
                        }                    
                    else if( m.type == DROP ) {
                        //Thread::staticSleep( 2000 );
                        
                        // send update even if action fails (to let them
                        // know that action is over)
                        playerIndicesToSendUpdatesAbout.push_back( i );

                        char canDrop = true;
                        
                        if( nextPlayer->holdingID > 0 &&
                            getObject( nextPlayer->holdingID )->permanent ) {
                            canDrop = false;
                            }

                        int target = getMapObject( m.x, m.y );
                        
                        
                        char accessBlocked = 
                            isAccessBlocked( nextPlayer, 
                                             m.x, m.y, target );
                        
                        if( ! accessBlocked )
                        if( isBiomeAllowedForPlayer( nextPlayer, m.x, m.y ) )
                        if( ( isGridAdjacent( m.x, m.y,
                                              nextPlayer->xd, 
                                              nextPlayer->yd ) 
                              ||
                              ( m.x == nextPlayer->xd &&
                                m.y == nextPlayer->yd )  ) ) {
                            
                            nextPlayer->actionAttempt = 1;
                            nextPlayer->actionTarget.x = m.x;
                            nextPlayer->actionTarget.y = m.y;
                            
                            if( m.x > nextPlayer->xd ) {
                                nextPlayer->facingOverride = 1;
                                }
                            else if( m.x < nextPlayer->xd ) {
                                nextPlayer->facingOverride = -1;
                                }

                            if( nextPlayer->holdingID != 0 ) {
                                
                                if( nextPlayer->holdingID < 0 ) {
                                    // baby drop
                                    
                                    if( target == 0 // nothing here
                                        ||
                                        ! getObject( target )->
                                            blocksWalking ) {
                                        handleDrop( 
                                            m.x, m.y, nextPlayer,
                                            &playerIndicesToSendUpdatesAbout );
                                        }    
                                    }
                                else if( canDrop && 
                                         isMapSpotEmpty( m.x, m.y ) ) {
                                
                                    // empty spot to drop non-baby into
                                    
                                    handleDrop( 
                                        m.x, m.y, nextPlayer,
                                        &playerIndicesToSendUpdatesAbout );
                                    }
                                else if( canDrop &&
                                         m.c >= 0 && 
                                         m.c < NUM_CLOTHING_PIECES &&
                                         m.x == nextPlayer->xd &&
                                         m.y == nextPlayer->yd  &&
                                         nextPlayer->holdingID > 0 ) {
                                    
                                    // drop into clothing indicates right-click
                                    // so swap
                                    
                                    int oldHeld = nextPlayer->holdingID;
                                    
                                    // first add to top of container
                                    // if possible
                                    addHeldToClothingContainer( nextPlayer,
                                                                m.c,
                                                                true );
                                    if( nextPlayer->holdingID == 0 ) {
                                        // add to top worked

                                        double playerAge = 
                                            computeAge( nextPlayer );
                                        
                                        // now take off bottom to hold
                                        // but keep looking to find something
                                        // different than what we were
                                        // holding before
                                        // AND that we are old enough to pick
                                        // up
                                        for( int s=0; 
                                             s < nextPlayer->
                                                 clothingContained[m.c].size() 
                                                 - 1;
                                             s++ ) {
                                            
                                            int otherID =
                                                nextPlayer->
                                                clothingContained[m.c].
                                                getElementDirect( s );
                                            
                                            if( otherID != 
                                                oldHeld &&
                                                canPickup( otherID, 
                                                           playerAge ) ) {
                                                
                                              removeFromClothingContainerToHold(
                                                    nextPlayer, m.c, s );
                                                break;
                                                }
                                            }
                                        
                                        // check to make sure remove worked
                                        // (otherwise swap failed)
                                        ObjectRecord *cObj = 
                                            clothingByIndex( 
                                                nextPlayer->clothing, m.c );
                                        if( nextPlayer->clothingContained[m.c].
                                            size() > cObj->numSlots ) {
                                            
                                            // over-full, remove failed
                                            
                                            // pop top item back off into hand
                                            removeFromClothingContainerToHold(
                                                nextPlayer, m.c, 
                                                nextPlayer->
                                                clothingContained[m.c].
                                                size() - 1 );
                                            }
                                        }
                                    
                                    }
                                else if( nextPlayer->holdingID > 0 ) {
                                    // non-baby drop
                                    
                                    ObjectRecord *droppedObj
                                        = getObject( 
                                            nextPlayer->holdingID );
                                    
                                    if( target != 0 ) {
                                        
                                        ObjectRecord *targetObj =
                                            getObject( target );
                                        

                                        if( !canDrop ) {
                                            // user may have a permanent object
                                            // stuck in their hand with no place
                                            // to drop it
                                            
                                            // need to check if 
                                            // a use-on-bare-ground
                                            // transition applies.  If so, we
                                            // can treat it like a swap

                                    
                                            if( ! targetObj->permanent
                                                &&
                                                canPickup( 
                                                    targetObj->id,
                                                    computeAge( 
                                                        nextPlayer ) ) ) {
                                                
                                                // target can be picked up

                                                // "set-down" type bare ground 
                                                // trans exists?
                                                TransRecord
                                                *r = getPTrans( 
                                                    nextPlayer->holdingID, 
                                                    -1 );

                                                if( r != NULL && 
                                                    r->newActor == 0 &&
                                                    r->newTarget > 0 ) {
                                            
                                                    // only applies if the 
                                                    // bare-ground
                                                    // trans leaves nothing in
                                                    // our hand
                                                    
                                                    // now swap it with the 
                                                    // non-permanent object
                                                    // on the ground.

                                                    swapHeldWithGround( 
                                                        nextPlayer,
                                                        target,
                                                        m.x,
                                                        m.y,
                                            &playerIndicesToSendUpdatesAbout );
                                                    }
                                                }
                                            }


                                        int targetSlots =
                                            targetObj->numSlots;
                                        
                                        float targetSlotSize = 0;
                                        
                                        if( targetSlots > 0 ) {
                                            targetSlotSize =
                                                targetObj->slotSize;
                                            }
                                        
                                        char canGoIn = false;
                                        
                                        if( canDrop &&
                                            droppedObj->containable &&
                                            targetSlotSize >=
                                            droppedObj->containSize ) {
                                            canGoIn = true;
                                            }
                                        
                                        

                                        // DROP indicates they 
                                        // right-clicked on container
                                        // so use swap mode
                                        if( canDrop && 
                                            canGoIn && 
                                            addHeldToContainer( 
                                                nextPlayer,
                                                target,
                                                m.x, m.y, true ) ) {
                                            // handled
                                            }
                                        else if( canDrop && 
                                                 ! canGoIn &&
                                                 targetObj->permanent &&
                                                 nextPlayer->numContained 
                                                 == 0 ) {
                                            // try treating it like
                                            // a USE action
                                            
                                            TransRecord *useTrans =
                                                getPTrans( 
                                                    nextPlayer->holdingID,
                                                    target );
                                            // handle simple case
                                            // stacking containers
                                            // client sends DROP for this
                                            if( useTrans != NULL &&
                                                useTrans->newActor == 0 ) {
                                                
                                                char canUse = true;
                                                
                                                ObjectRecord *newTargetObj = 
                                                    NULL;
                                                
                                                if( useTrans->newTarget > 0 ) {
                                                    newTargetObj =
                                                        getObject(
                                                            useTrans->
                                                            newTarget );
                                                    }

                                                if( newTargetObj != NULL &&
                                                    newTargetObj->
                                                    isBiomeLimited &&
                                                    ! canBuildInBiome( 
                                                        newTargetObj,
                                                        getMapBiome( m.x,
                                                                     m.y ) ) ) {
                                                    canUse = false;
                                                    }

                                                if( canUse ) {
                                                    handleHoldingChange(
                                                        nextPlayer,
                                                        useTrans->newActor );
                                                    
                                                    setMapObject( 
                                                        m.x, m.y,
                                                        useTrans->newTarget );
                                                    }
                                                }
                                            }
                                        else if( canDrop && 
                                                 ! canGoIn &&
                                                 ! targetObj->permanent 
                                                 &&
                                                 canPickup( 
                                                     targetObj->id,
                                                     computeAge( 
                                                         nextPlayer ) ) ) {
                                            // drop onto a spot where
                                            // something exists, and it's
                                            // not a container

                                            // swap what we're holding for
                                            // target
                                            
                                            int oldHeld = 
                                                nextPlayer->holdingID;
                                            int oldNumContained =
                                                nextPlayer->numContained;
                                            
                                            // now swap
                                            swapHeldWithGround( 
                                             nextPlayer, target, m.x, m.y,
                                             &playerIndicesToSendUpdatesAbout );
                                            
                                            if( oldHeld == 
                                                nextPlayer->holdingID &&
                                                oldNumContained ==
                                                nextPlayer->numContained ) {
                                                // no change
                                                // are they the same object?
                                                if( oldNumContained == 0 && 
                                                    oldHeld == target ) {
                                                    // try using empty held
                                                    // on target
                                                    TransRecord *sameTrans
                                                        = getPTrans(
                                                            oldHeld, target );
                                                    if( sameTrans != NULL &&
                                                        sameTrans->newActor ==
                                                        0 ) {
                                                        // keep it simple
                                                        // for now
                                                        // this is usually
                                                        // just about
                                                        // stacking
                                                        handleHoldingChange(
                                                            nextPlayer,
                                                            sameTrans->
                                                            newActor );
                                                        
                                                        setMapObject(
                                                            m.x, m.y,
                                                            sameTrans->
                                                            newTarget );
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    else if( canDrop ) {
                                        // no object here
                                        
                                        // maybe there's a person
                                        // standing here

                                        // only allow drop if what we're
                                        // dropping is non-blocking
                                        
                                        
                                        if( ! droppedObj->blocksWalking ) {
                                            
                                             handleDrop( 
                                              m.x, m.y, nextPlayer,
                                              &playerIndicesToSendUpdatesAbout 
                                              );
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    else if( m.type == REMV ) {
                        // send update even if action fails (to let them
                        // know that action is over)
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        if( isBiomeAllowedForPlayer( nextPlayer, m.x, m.y ) )
                        if( isGridAdjacent( m.x, m.y, 
                                            nextPlayer->xd, 
                                            nextPlayer->yd ) 
                            ||
                            ( m.x == nextPlayer->xd &&
                              m.y == nextPlayer->yd ) ) {
                            
                            int target = getMapObject( m.x, m.y );

                            char accessBlocked =
                                isAccessBlocked( nextPlayer, m.x, m.y, target );
                            

                            char handEmpty = ( nextPlayer->holdingID == 0 );

                            if( ! accessBlocked )                        
                            removeFromContainerToHold( nextPlayer,
                                                       m.x, m.y, m.i );

                            if( ! accessBlocked )
                            if( handEmpty &&
                                nextPlayer->holdingID == 0 ) {
                                // hand still empty?
                            
                                int target = getMapObject( m.x, m.y );

                                if( target > 0 ) {
                                    ObjectRecord *targetObj = 
                                        getObject( target );
                                
                                    if( ! targetObj->permanent &&
                                        canPickup( targetObj->id,
                                                   computeAge( 
                                                       nextPlayer ) ) ) {
                                    
                                        // treat it like pick up   
                                        pickupToHold( nextPlayer, m.x, m.y, 
                                                      target );
                                        }
                                    else if( targetObj->permanent ) {
                                        // consider bare-hand action
                                        TransRecord *handTrans = getPTrans(
                                            0, target );
                                    
                                        // handle only simplest case here
                                        // (to avoid side-effects)
                                        // REMV on container stack
                                        // (make sure they have the same
                                        //  use parent)
                                        if( handTrans != NULL &&
                                            handTrans->newTarget > 0 &&
                                            getObject( handTrans->newTarget )->
                                            numSlots == targetObj->numSlots &&
                                            handTrans->newActor > 0 &&
                                            canPickup( 
                                                handTrans->newActor,
                                                computeAge( nextPlayer ) ) ) {
                                        
                                            handleHoldingChange( 
                                                nextPlayer,
                                                handTrans->newActor );
                                            setMapObject( 
                                                m.x, m.y, 
                                                handTrans->newTarget );
                                            }
                                        }
                                    }
                                }
                            }
                        }                        
                    else if( m.type == SREMV ) {
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        // remove contained object from clothing
                        char worked = false;
                        
                        if( m.x == nextPlayer->xd &&
                            m.y == nextPlayer->yd &&
                            nextPlayer->holdingID == 0 ) {
                            
                            nextPlayer->actionAttempt = 1;
                            nextPlayer->actionTarget.x = m.x;
                            nextPlayer->actionTarget.y = m.y;
                            
                            if( m.c >= 0 && m.c < NUM_CLOTHING_PIECES ) {
                                worked = removeFromClothingContainerToHold(
                                    nextPlayer, m.c, m.i );
                                }
                            }
                        
                        if( nextPlayer->holdingID == 0 && 
                            m.c >= 0 && m.c < NUM_CLOTHING_PIECES  &&
                            ! worked ) {

                            // hmm... nothing to remove from slots in clothing
                            
                            // player is right-clicking, and maybe they
                            // can't left-click, because there's a 
                            // transition in the way
                            
                            // if so, right click should
                            // remove the clothing itself
                            
                            ObjectRecord **clothingSlot = 
                                getClothingSlot( nextPlayer, m.c );


                            TransRecord *bareHandClothingTrans =
                                getBareHandClothingTrans( nextPlayer,
                                                          clothingSlot );
                                
                            if( bareHandClothingTrans != NULL ) {
                                // there's a transition blocking
                                // regular-click to remove empty
                                // clothing.
                                // allow right click to do it

                                removeClothingToHold( nextPlayer,
                                                      nextPlayer,
                                                      clothingSlot,
                                                      m.c );
                                }
                            }
                        }
                    else if( m.type == EMOT && 
                             ! nextPlayer->emotFrozen ) {
                        // ignore new EMOT requres from player if emot
                        // frozen
                        
                        if( m.i <= SettingsManager::getIntSetting( 
                                "allowedEmotRange", 6 ) ) {
                            
                            SimpleVector<int> *forbidden =
                                SettingsManager::getIntSettingMulti( 
                                    "forbiddenEmots" );
                            
                            if( forbidden->getElementIndex( m.i ) == -1 ) {
                                // not forbidden
                                
                                double curTime = Time::getCurrentTime();
                                
                                char cooldown = false;
                                
                                if( nextPlayer->emoteCooldown ) {
                                    if( curTime - 
                                        nextPlayer->
                                        emoteCooldownStartTimeSeconds >
                                        emoteCooldownSeconds ) {
                                        // cooldown over
                                        nextPlayer->emoteCooldown = false;
                                        nextPlayer->firstEmoteTimeSeconds =
                                            curTime;
                                        nextPlayer->emoteCountInWindow = 0;
                                        }
                                    else {
                                        cooldown = true;
                                        }
                                    }
                                
                                if( ! cooldown ) {
                                    // fire off emote
                                    newEmotPlayerIDs.push_back( 
                                        nextPlayer->id );
                                    newEmotIndices.push_back( m.i );
                                    // player-requested emots have 
                                    // no specific TTL
                                    newEmotTTLs.push_back( 0 );

                                    // now see if cooldown has been triggered
                                    if( curTime - 
                                        nextPlayer->firstEmoteTimeSeconds
                                        > emoteWindowSeconds ) {
                                        // window expired
                                        // start a new one
                                        nextPlayer->firstEmoteTimeSeconds =
                                            curTime;
                                        nextPlayer->emoteCountInWindow = 0;
                                        }
                                    else {
                                        // in window time
                                        nextPlayer->emoteCountInWindow ++;
                                        
                                        if( nextPlayer->emoteCountInWindow >
                                            maxEmotesInWindow ) {
                                            // put 'em on cooldown
                                            nextPlayer->emoteCooldown = true;
                                            nextPlayer->
                                                emoteCooldownStartTimeSeconds =
                                                curTime;
                                            }
                                        }
                                    }
                                }
                            delete forbidden;
                            }
                        } 
                    }
                
                if( m.numExtraPos > 0 ) {
                    delete [] m.extraPos;
                    }
                
                if( m.saidText != NULL ) {
                    delete [] m.saidText;
                    }
                if( m.bugText != NULL ) {
                    delete [] m.bugText;
                    }
                }
            }

        
        // process pending KILL actions
        for( int i=0; i<activeKillStates.size(); i++ ) {
            KillState *s = activeKillStates.getElement( i );
            
            LiveObject *killer = getLiveObject( s->killerID );
            LiveObject *target = getLiveObject( s->targetID );
            
            if( killer == NULL || target == NULL ||
                killer->error || target->error ||
                killer->holdingID != s->killerWeaponID ||
                target->heldByOther ) {
                // either player dead, or held-weapon change
                // or target baby now picked up (safe)
                
                // kill request done
                
                removeKillState( killer, target );

                i--;
                continue;
                }
            
            // kill request still active!
            
            // see if it is realized (close enough)?
            GridPos playerPos = getPlayerPos( killer );
            GridPos targetPos = getPlayerPos( target );
            
            double dist = distance( playerPos, targetPos );
            
            double curTime = Time::getCurrentTime();

            if( curTime - s->killStartTime  > killDelayTime && 
                getObject( killer->holdingID )->deadlyDistance >= dist &&
                ! directLineBlocked( playerPos, targetPos ) ) {
                // enough warning time has passed
                // and
                // close enough to kill
                
                executeKillAction( getLiveObjectIndex( s->killerID ),
                                   getLiveObjectIndex( s->targetID ),
                                   &playerIndicesToSendUpdatesAbout,
                                   &playerIndicesToSendDyingAbout,
                                   &newEmotPlayerIDs,
                                   &newEmotIndices,
                                   &newEmotTTLs );
                }
            else {
                // still not close enough
                // see if we need to renew emote
                
                if( curTime - s->emotStartTime > s->emotRefreshSeconds ) {
                    s->emotStartTime = curTime;
                    
                    // refresh again in 30 seconds, even if we had a shorter
                    // refresh time because of an intervening emot
                    s->emotRefreshSeconds = 30;

                    newEmotPlayerIDs.push_back( killer->id );
                            
                    newEmotIndices.push_back( killEmotionIndex );
                    newEmotTTLs.push_back( 120 );

                    newEmotPlayerIDs.push_back( target->id );
                            
                    newEmotIndices.push_back( victimEmotionIndex );
                    newEmotTTLs.push_back( 120 );
                    }
                }
            }
        


        // now that messages have been processed for all
        // loop over and handle all post-message checks

        // for example, if a player later in the list sends a message
        // killing an earlier player, we need to check to see that
        // player deleted afterward here
        for( int i=0; i<numLive; i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            
            double curTime = Time::getCurrentTime();
            
            
            if( nextPlayer->emotFrozen && 
                nextPlayer->emotUnfreezeETA != 0 &&
                curTime >= nextPlayer->emotUnfreezeETA ) {
                
                nextPlayer->emotFrozen = false;
                nextPlayer->emotUnfreezeETA = 0;
                }
            

            if( nextPlayer->dying && ! nextPlayer->error &&
                curTime >= nextPlayer->dyingETA ) {
                // finally died
                nextPlayer->error = true;

                
                if( ! nextPlayer->isTutorial ) {
                    GridPos deathPos = 
                        getPlayerPos( nextPlayer );
                    logDeath( nextPlayer->id,
                              nextPlayer->email,
                              nextPlayer->isEve,
                              computeAge( nextPlayer ),
                              getSecondsPlayed( 
                                  nextPlayer ),
                              ! getFemale( nextPlayer ),
                              deathPos.x, deathPos.y,
                              players.size() - 1,
                              false,
                              nextPlayer->murderPerpID,
                              nextPlayer->murderPerpEmail );
                                            
                    if( shutdownMode ) {
                        handleShutdownDeath( 
                            nextPlayer, nextPlayer->xd, nextPlayer->yd );
                        }
                    }
                
                nextPlayer->deathLogged = true;
                }
            

                
            if( nextPlayer->isNew ) {
                // their first position is an update
                

                playerIndicesToSendUpdatesAbout.push_back( i );
                playerIndicesToSendLineageAbout.push_back( i );
                
                
                if( nextPlayer->curseStatus.curseLevel > 0 ) {
                    playerIndicesToSendCursesAbout.push_back( i );
                    }

                nextPlayer->isNew = false;
                
                // force this PU to be sent to everyone
                nextPlayer->updateGlobal = true;
                }
            else if( nextPlayer->error && ! nextPlayer->deleteSent ) {
                
                // generate log line whenever player dies
                logFamilyCounts();
                
                
                // check if we should send global message about a family's
                // demise
                if( ! nextPlayer->isTutorial && 
                    nextPlayer->curseStatus.curseLevel == 0 &&
                    ! isEveWindow() ) {
                    int minFamiliesAfterEveWindow =
                        SettingsManager::getIntSetting( 
                            "minFamiliesAfterEveWindow", 5 );
                    if( minFamiliesAfterEveWindow > 0 ) {
                        // is this the last player of this family?

                        if( nextPlayer->familyName != NULL ) {
                            int otherCount = 0;
                            
                            for( int n=0; n<players.size(); n++ ) {
                                LiveObject *otherPlayer =
                                    players.getElement( n );
                                
                                if( otherPlayer->error ) {
                                    // don't worry about counting
                                    // nextPlayer here, b/c they have an
                                    // error set already
                                    continue;
                                    }
                                if( otherPlayer->lineageEveID ==
                                    nextPlayer->lineageEveID ) {
                                    
                                    otherCount++;
                                    // actually, only need to count 1
                                    break;
                                    }
                                }
                            if( otherCount == 0 ) {
                                // family died out!
                                int cFam = countFamilies();
                                
                                const char *famWord = "FAMILIES";
                                if( cFam == 1 ) {
                                    famWord = "FAMILY";
                                    }

                                char *message = 
                                    autoSprintf( "%s FAMILY JUST DIED OUT**"
                                                 "%d %s LEFT "
                                                 "(ARC ENDS BELOW %d)",
                                                 nextPlayer->familyName,
                                                 cFam,
                                                 famWord,
                                                 minFamiliesAfterEveWindow );
                                
                                sendGlobalMessage( message );
                                delete [] message;
                                }
                            }
                        }
                    }
                
                leaderDied( nextPlayer );

                removeAllOwnership( nextPlayer );
                
                decrementLanguageCount( nextPlayer->lineageEveID );
                
                removePlayerLanguageMaps( nextPlayer->id );
                
                if( nextPlayer->heldByOther ) {
                    
                    handleForcedBabyDrop( nextPlayer,
                                          &playerIndicesToSendUpdatesAbout );
                    }                
                else if( nextPlayer->holdingID < 0 ) {
                    LiveObject *babyO = 
                        getLiveObject( - nextPlayer->holdingID );
                    
                    handleForcedBabyDrop( babyO,
                                          &playerIndicesToSendUpdatesAbout );
                    }
                

                newDeleteUpdates.push_back( 
                    getUpdateRecord( nextPlayer, true ) );                
                
                nextPlayer->deathTimeSeconds = Time::getCurrentTime();

                nextPlayer->isNew = false;
                
                nextPlayer->deleteSent = true;
                // wait 5 seconds before closing their connection
                // so they can get the message
                nextPlayer->deleteSentDoneETA = Time::getCurrentTime() + 5;
                
                if( areTriggersEnabled() ) {
                    // add extra time so that rest of triggers can be received
                    // and rest of trigger results can be sent
                    // back to this client
                    
                    // another hour...
                    nextPlayer->deleteSentDoneETA += 3600;
                    // and don't set their error flag after all
                    // keep receiving triggers from them

                    nextPlayer->error = false;
                    }
                else {
                    if( nextPlayer->sock != NULL ) {
                        // stop listening for activity on this socket
                        sockPoll.removeSocket( nextPlayer->sock );
                        }
                    }
                

                GridPos dropPos;
                
                if( nextPlayer->xd == 
                    nextPlayer->xs &&
                    nextPlayer->yd ==
                    nextPlayer->ys ) {
                    // deleted player standing still
                    
                    dropPos.x = nextPlayer->xd;
                    dropPos.y = nextPlayer->yd;
                    }
                else {
                    // player moving
                    
                    dropPos = 
                        computePartialMoveSpot( nextPlayer );
                    }
                
                // report to lineage server once here
                double age = computeAge( nextPlayer );
                
                int killerID = -1;
                if( nextPlayer->murderPerpID > 0 ) {
                    killerID = nextPlayer->murderPerpID;
                    }
                else if( nextPlayer->deathSourceID > 0 ) {
                    // include as negative of ID
                    killerID = - nextPlayer->deathSourceID;
                    }
                else if( nextPlayer->suicide ) {
                    // self id is killer
                    killerID = nextPlayer->id;
                    }
                
                
                
                char male = ! getFemale( nextPlayer );
                
                if( ! nextPlayer->isTutorial )
                recordPlayerLineage( nextPlayer->email, 
                                     age,
                                     nextPlayer->id,
                                     nextPlayer->parentID,
                                     nextPlayer->displayID,
                                     killerID,
                                     nextPlayer->name,
                                     nextPlayer->lastSay,
                                     male );


                // both tutorial and non-tutorial players
                logFitnessDeath( nextPlayer );
                


                if( SettingsManager::getIntSetting( 
                        "babyApocalypsePossible", 1 ) 
                    &&
                    players.size() > 
                    SettingsManager::getIntSetting(
                        "minActivePlayersForBabyApocalypse", 15 ) ) {
                    
                    double curTime = Time::getCurrentTime();
                    
                    if( ! nextPlayer->isEve ) {
                    
                        // player was born as a baby
                        
                        int barrierRadius = 
                            SettingsManager::getIntSetting( 
                                "barrierRadius", 250 );
                        int barrierOn = SettingsManager::getIntSetting( 
                            "barrierOn", 1 );

                        char insideBarrier = true;
                        
                        if( barrierOn &&
                            ( abs( dropPos.x ) > barrierRadius ||
                              abs( dropPos.y ) > barrierRadius ) ) {
                            
                            insideBarrier = false;
                            }
                              

                        float threshold = SettingsManager::getFloatSetting( 
                            "babySurvivalYearsBeforeApocalypse", 15.0f );
                        
                        if( insideBarrier && age > threshold ) {
                            // baby passed threshold, update last-passed time
                            lastBabyPassedThresholdTime = curTime;
                            }
                        else {
                            // baby died young
                            // OR older, outside barrier
                            // check if we're due for an apocalypse
                            
                            if( lastBabyPassedThresholdTime > 0 &&
                                curTime - lastBabyPassedThresholdTime >
                                SettingsManager::getIntSetting(
                                    "babySurvivalWindowSecondsBeforeApocalypse",
                                    3600 ) ) {
                                // we're outside the window
                                // people have been dying young for a long time
                                
                                triggerApocalypseNow( 
                                    "Everyone dying young for too long" );
                                }
                            else if( lastBabyPassedThresholdTime == 0 ) {
                                // first baby to die, and we have enough
                                // active players.
                                
                                // start window now
                                lastBabyPassedThresholdTime = curTime;
                                }
                            }
                        }
                    }
                else {
                    // not enough players
                    // reset window
                    lastBabyPassedThresholdTime = curTime;
                    }
                

                // don't use age here, because it unfairly gives Eve
                // +14 years that she didn't actually live
                // use true played years instead
                double yearsLived = 
                    getSecondsPlayed( nextPlayer ) * getAgeRate();

                if( ! nextPlayer->isTutorial ) {
                    
                    recordLineage( 
                        nextPlayer->email, 
                        nextPlayer->originalBirthPos,
                        yearsLived, 
                        // count true murder victims here, not suicide
                        ( killerID > 0 && killerID != nextPlayer->id ),
                        // killed other or committed SID suicide
                        nextPlayer->everKilledAnyone || 
                        nextPlayer->suicide );
        
                    if( nextPlayer->suicide ) {
                        // add to player's skip list
                        skipFamily( nextPlayer->email, 
                                    nextPlayer->lineageEveID );
                        }
                    }
                
                

                if( ! nextPlayer->deathLogged ) {
                    char disconnect = true;
                    
                    if( age >= forceDeathAge ) {
                        disconnect = false;
                        }
                    
                    if( ! nextPlayer->isTutorial ) {    
                        logDeath( nextPlayer->id,
                                  nextPlayer->email,
                                  nextPlayer->isEve,
                                  age,
                                  getSecondsPlayed( nextPlayer ),
                                  male,
                                  dropPos.x, dropPos.y,
                                  players.size() - 1,
                                  disconnect );
                    
                        if( shutdownMode ) {
                            handleShutdownDeath( 
                                nextPlayer, dropPos.x, dropPos.y );
                            }
                        }
                    
                    nextPlayer->deathLogged = true;
                    }
                
                // now that death has been logged, and delete sent,
                // we can clear their email address so that the 
                // can log in again during the deleteSentDoneETA window
                
                if( nextPlayer->email != NULL ) {
                    if( nextPlayer->origEmail != NULL ) {
                        delete [] nextPlayer->origEmail;
                        }
                    nextPlayer->origEmail = 
                        stringDuplicate( nextPlayer->email );
                    delete [] nextPlayer->email;
                    }
                nextPlayer->email = stringDuplicate( "email_cleared" );

                int deathID = getRandomDeathMarker();
                    
                if( nextPlayer->customGraveID > -1 ) {
                    deathID = nextPlayer->customGraveID;
                    }

                char deathMarkerHasSlots = false;
                
                if( deathID > 0 ) {
                    deathMarkerHasSlots = 
                        ( getObject( deathID )->numSlots > 0 );
                    }

                int oldObject = getMapObject( dropPos.x, dropPos.y );
                
                SimpleVector<int> oldContained;
                SimpleVector<timeSec_t> oldContainedETADecay;
                
                if( deathID != 0 ) {
                    
                
                    int nX[4] = { -1, 1,  0, 0 };
                    int nY[4] = {  0, 0, -1, 1 };
                    
                    int n = 0;
                    GridPos centerDropPos = dropPos;
                    
                    while( oldObject != 0 && n < 4 ) {
                        
                        // don't combine graves
                        if( ! isGrave( oldObject ) ) {
                            ObjectRecord *r = getObject( oldObject );
                            
                            if( deathMarkerHasSlots &&
                                r->numSlots == 0 && ! r->permanent 
                                && ! r->rideable ) {
                                
                                // found a containble object
                                // we can empty this spot to make room
                                // for a grave that can go here, and
                                // put the old object into the new grave.
                                
                                oldContained.push_back( oldObject );
                                oldContainedETADecay.push_back(
                                    getEtaDecay( dropPos.x, dropPos.y ) );
                                
                                setMapObject( dropPos.x, dropPos.y, 0 );
                                oldObject = 0;
                                }
                            }
                        
                        oldObject = getMapObject( dropPos.x, dropPos.y );
                        
                        if( oldObject != 0 ) {
                            
                            // try next neighbor
                            dropPos.x = centerDropPos.x + nX[n];
                            dropPos.y = centerDropPos.y + nY[n];
                            
                            n++;
                            oldObject = getMapObject( dropPos.x, dropPos.y );
                            }
                        }
                    }
                

                if( ! isMapSpotEmpty( dropPos.x, dropPos.y, false ) ) {
                    
                    // failed to find an empty spot, or a containable object
                    // at center or four neighbors
                    
                    // search outward in spiral of up to 100 points
                    // look for some empty spot
                    
                    char foundEmpty = false;
                    
                    GridPos newDropPos = findClosestEmptyMapSpot(
                        dropPos.x, dropPos.y, 100, &foundEmpty );
                    
                    if( foundEmpty ) {
                        dropPos = newDropPos;
                        }
                    }


                // assume death markes non-blocking, so it's safe
                // to drop one even if other players standing here
                if( isMapSpotEmpty( dropPos.x, dropPos.y, false ) ) {

                    if( deathID > 0 ) {
                        
                        setResponsiblePlayer( - nextPlayer->id );
                        setMapObject( dropPos.x, dropPos.y, 
                                      deathID );
                        setResponsiblePlayer( -1 );
                        
                        GraveInfo graveInfo = { dropPos, nextPlayer->id,
                                                nextPlayer->lineageEveID };
                        newGraves.push_back( graveInfo );
                        
                        setGravePlayerID( dropPos.x, dropPos.y,
                                          nextPlayer->id );

                        ObjectRecord *deathObject = getObject( deathID );
                        
                        int roomLeft = deathObject->numSlots;
                        
                        if( roomLeft >= 1 ) {
                            // room for weapon remnant
                            if( nextPlayer->embeddedWeaponID != 0 ) {
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->embeddedWeaponID,
                                    nextPlayer->embeddedWeaponEtaDecay );
                                roomLeft--;
                                }
                            }
                        
                            
                        if( roomLeft >= 5 ) {
                            // room for clothing
                            
                            if( nextPlayer->clothing.tunic != NULL ) {
                                
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->clothing.tunic->id,
                                    nextPlayer->clothingEtaDecay[1] );
                                roomLeft--;
                                }
                            if( nextPlayer->clothing.bottom != NULL ) {
                                
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->clothing.bottom->id,
                                    nextPlayer->clothingEtaDecay[4] );
                                roomLeft--;
                                }
                            if( nextPlayer->clothing.backpack != NULL ) {
                                
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->clothing.backpack->id,
                                    nextPlayer->clothingEtaDecay[5] );
                                roomLeft--;
                                }
                            if( nextPlayer->clothing.backShoe != NULL ) {
                                
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->clothing.backShoe->id,
                                    nextPlayer->clothingEtaDecay[3] );
                                roomLeft--;
                                }
                            if( nextPlayer->clothing.frontShoe != NULL ) {
                                
                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->clothing.frontShoe->id,
                                    nextPlayer->clothingEtaDecay[2] );
                                roomLeft--;
                                }
                            if( nextPlayer->clothing.hat != NULL ) {
                                
                                addContained( dropPos.x, dropPos.y,
                                              nextPlayer->clothing.hat->id,
                                              nextPlayer->clothingEtaDecay[0] );
                                roomLeft--;
                                }
                            }
                        
                        // room for what clothing contained
                        timeSec_t curTime = Time::timeSec();
                        
                        for( int c=0; c < NUM_CLOTHING_PIECES && roomLeft > 0; 
                             c++ ) {
                            
                            float oldStretch = 1.0;
                            
                            ObjectRecord *cObj = clothingByIndex( 
                                nextPlayer->clothing, c );
                            
                            if( cObj != NULL ) {
                                oldStretch = cObj->slotTimeStretch;
                                }
                            
                            float newStretch = deathObject->slotTimeStretch;
                            
                            for( int cc=0; 
                                 cc < nextPlayer->clothingContained[c].size() 
                                     &&
                                     roomLeft > 0;
                                 cc++ ) {
                                
                                if( nextPlayer->
                                    clothingContainedEtaDecays[c].
                                    getElementDirect( cc ) != 0 &&
                                    oldStretch != newStretch ) {
                                        
                                    timeSec_t offset = 
                                        nextPlayer->
                                        clothingContainedEtaDecays[c].
                                        getElementDirect( cc ) - 
                                        curTime;
                                        
                                    offset = offset * oldStretch;
                                    offset = offset / newStretch;
                                        
                                    *( nextPlayer->
                                       clothingContainedEtaDecays[c].
                                       getElement( cc ) ) =
                                        curTime + offset;
                                    }

                                addContained( 
                                    dropPos.x, dropPos.y,
                                    nextPlayer->
                                    clothingContained[c].
                                    getElementDirect( cc ),
                                    nextPlayer->
                                    clothingContainedEtaDecays[c].
                                    getElementDirect( cc ) );
                                roomLeft --;
                                }
                            }
                        
                        int oc = 0;
                        
                        while( oc < oldContained.size() && roomLeft > 0 ) {
                            addContained( 
                                dropPos.x, dropPos.y,
                                oldContained.getElementDirect( oc ),
                                oldContainedETADecay.getElementDirect( oc ) );
                            oc++;
                            roomLeft--;                                
                            }
                        }  
                    }
                if( nextPlayer->holdingID != 0 ) {

                    char doNotDrop = false;
                    
                    if( nextPlayer->murderSourceID > 0 ) {
                        
                        TransRecord *woundHit = 
                            getPTrans( nextPlayer->murderSourceID, 
                                      0, true, false );
                        
                        if( woundHit != NULL &&
                            woundHit->newTarget > 0 ) {
                            
                            if( nextPlayer->holdingID == woundHit->newTarget ) {
                                // they are simply holding their wound object
                                // don't drop this on the ground
                                doNotDrop = true;
                                }
                            }
                        }
                    if( nextPlayer->holdingWound ||
                        nextPlayer->holdingBiomeSickness ) {
                        // holding a wound from some other, non-murder cause
                        // of death
                        doNotDrop = true;
                        }
                    
                    
                    if( ! doNotDrop ) {
                        // drop what they were holding

                        // this will almost always involve a throw
                        // (death marker, at least, will be in the way)
                        handleDrop( 
                            dropPos.x, dropPos.y, 
                            nextPlayer,
                            &playerIndicesToSendUpdatesAbout );
                        }
                    else {
                        // just clear what they were holding
                        nextPlayer->holdingID = 0;
                        }
                    }
                }
            else if( ! nextPlayer->error ) {
                // other update checks for living players
                
                if( nextPlayer->holdingEtaDecay != 0 &&
                    nextPlayer->holdingEtaDecay < curTime ) {
                
                    // what they're holding has decayed

                    int oldID = nextPlayer->holdingID;
                
                    TransRecord *t = getPTrans( -1, oldID );

                    if( t != NULL ) {

                        int newID = t->newTarget;
                        
                        handleHoldingChange( nextPlayer, newID );
                        
                        if( newID == 0 &&
                            nextPlayer->holdingWound &&
                            nextPlayer->dying ) {
                            
                            // wound decayed naturally, count as healed
                            setNoLongerDying( 
                                nextPlayer,
                                &playerIndicesToSendHealingAbout );            
                            }
                        

                        nextPlayer->heldTransitionSourceID = -1;
                        
                        ObjectRecord *newObj = getObject( newID );
                        ObjectRecord *oldObj = getObject( oldID );
                        
                        
                        if( newObj != NULL && newObj->permanent &&
                            oldObj != NULL && ! oldObj->permanent ) {
                            // object decayed into a permanent
                            // force drop
                             GridPos dropPos = 
                                getPlayerPos( nextPlayer );
                            
                             handleDrop( 
                                    dropPos.x, dropPos.y, 
                                    nextPlayer,
                                    &playerIndicesToSendUpdatesAbout );
                            }
                        

                        playerIndicesToSendUpdatesAbout.push_back( i );
                        }
                    else {
                        // no decay transition exists
                        // clear it
                        setFreshEtaDecayForHeld( nextPlayer );
                        }
                    }

                // check if anything in the container they are holding
                // has decayed
                if( nextPlayer->holdingID > 0 &&
                    nextPlayer->numContained > 0 ) {
                    
                    char change = false;
                    
                    SimpleVector<int> newContained;
                    SimpleVector<timeSec_t> newContainedETA;

                    SimpleVector< SimpleVector<int> > newSubContained;
                    SimpleVector< SimpleVector<timeSec_t> > newSubContainedETA;
                    
                    for( int c=0; c< nextPlayer->numContained; c++ ) {
                        int oldID = abs( nextPlayer->containedIDs[c] );
                        int newID = oldID;

                        timeSec_t newDecay = 
                            nextPlayer->containedEtaDecays[c];

                        SimpleVector<int> subCont = 
                            nextPlayer->subContainedIDs[c];
                        SimpleVector<timeSec_t> subContDecay = 
                            nextPlayer->subContainedEtaDecays[c];

                        if( newDecay != 0 && newDecay < curTime ) {
                            
                            change = true;
                            
                            TransRecord *t = getPTrans( -1, oldID );

                            newDecay = 0;

                            if( t != NULL ) {
                                
                                newID = t->newTarget;
                            
                                if( newID != 0 ) {
                                    float stretch = 
                                        getObject( nextPlayer->holdingID )->
                                        slotTimeStretch;
                                    
                                    TransRecord *newDecayT = 
                                        getMetaTrans( -1, newID );
                                
                                    if( newDecayT != NULL ) {
                                        newDecay = 
                                            Time::timeSec() +
                                            newDecayT->autoDecaySeconds /
                                            stretch;
                                        }
                                    else {
                                        // no further decay
                                        newDecay = 0;
                                        }
                                    }
                                }
                            }
                        
                        SimpleVector<int> cVec;
                        SimpleVector<timeSec_t> dVec;

                        if( newID != 0 ) {
                            int oldSlots = subCont.size();
                            
                            int newSlots = getObject( newID )->numSlots;
                            
                            if( newID != oldID
                                &&
                                newSlots < oldSlots ) {
                                
                                // shrink sub-contained
                                // this involves items getting lost
                                // but that's okay for now.
                                subCont.shrink( newSlots );
                                subContDecay.shrink( newSlots );
                                }
                            }
                        else {
                            subCont.deleteAll();
                            subContDecay.deleteAll();
                            }

                        // handle decay for each sub-contained object
                        for( int s=0; s<subCont.size(); s++ ) {
                            int oldSubID = subCont.getElementDirect( s );
                            int newSubID = oldSubID;
                            timeSec_t newSubDecay = 
                                subContDecay.getElementDirect( s );
                            
                            if( newSubDecay != 0 && newSubDecay < curTime ) {
                            
                                change = true;
                            
                                TransRecord *t = getPTrans( -1, oldSubID );

                                newSubDecay = 0;

                                if( t != NULL ) {
                                
                                    newSubID = t->newTarget;
                            
                                    if( newSubID != 0 ) {
                                        float subStretch = 
                                            getObject( newID )->
                                            slotTimeStretch;
                                    
                                        TransRecord *newSubDecayT = 
                                            getMetaTrans( -1, newSubID );
                                
                                        if( newSubDecayT != NULL ) {
                                            newSubDecay = 
                                                Time::timeSec() +
                                                newSubDecayT->autoDecaySeconds /
                                                subStretch;
                                            }
                                        else {
                                            // no further decay
                                            newSubDecay = 0;
                                            }
                                        }
                                    }
                                }
                            
                            if( newSubID != 0 ) {
                                cVec.push_back( newSubID );
                                dVec.push_back( newSubDecay );
                                }
                            }
                        
                        if( newID != 0 ) {    
                            newSubContained.push_back( cVec );
                            newSubContainedETA.push_back( dVec );

                            if( cVec.size() > 0 ) {
                                newID *= -1;
                                }
                            
                            newContained.push_back( newID );
                            newContainedETA.push_back( newDecay );
                            }
                        }
                    
                    

                    if( change ) {
                        playerIndicesToSendUpdatesAbout.push_back( i );
                        
                        freePlayerContainedArrays( nextPlayer );
                        
                        nextPlayer->numContained = newContained.size();

                        if( nextPlayer->numContained == 0 ) {
                            nextPlayer->containedIDs = NULL;
                            nextPlayer->containedEtaDecays = NULL;
                            nextPlayer->subContainedIDs = NULL;
                            nextPlayer->subContainedEtaDecays = NULL;
                            }
                        else {
                            nextPlayer->containedIDs = 
                                newContained.getElementArray();
                            nextPlayer->containedEtaDecays = 
                                newContainedETA.getElementArray();
                        
                            nextPlayer->subContainedIDs =
                                newSubContained.getElementArray();
                            nextPlayer->subContainedEtaDecays =
                                newSubContainedETA.getElementArray();
                            }
                        }
                    }
                
                
                // check if their clothing has decayed
                // or what's in their clothing
                for( int c=0; c<NUM_CLOTHING_PIECES; c++ ) {
                    ObjectRecord *cObj = 
                        clothingByIndex( nextPlayer->clothing, c );
                    
                    if( cObj != NULL &&
                        nextPlayer->clothingEtaDecay[c] != 0 &&
                        nextPlayer->clothingEtaDecay[c] < 
                        curTime ) {
                
                        // what they're wearing has decayed

                        int oldID = cObj->id;
                
                        TransRecord *t = getPTrans( -1, oldID );

                        if( t != NULL ) {

                            int newID = t->newTarget;
                            
                            ObjectRecord *newCObj = NULL;
                            if( newID != 0 ) {
                                newCObj = getObject( newID );
                                
                                TransRecord *newDecayT = 
                                    getMetaTrans( -1, newID );
                                
                                if( newDecayT != NULL ) {
                                    nextPlayer->clothingEtaDecay[c] = 
                                        Time::timeSec() + 
                                        newDecayT->autoDecaySeconds;
                                    }
                                else {
                                    // no further decay
                                    nextPlayer->clothingEtaDecay[c] = 0;
                                    }
                                }
                            else {
                                nextPlayer->clothingEtaDecay[c] = 0;
                                }
                            
                            setClothingByIndex( &( nextPlayer->clothing ),
                                                c, newCObj );
                            
                            int oldSlots = 
                                nextPlayer->clothingContained[c].size();

                            int newSlots = getNumContainerSlots( newID );
                    
                            if( newSlots < oldSlots ) {
                                // new container can hold less
                                // truncate
                                
                                // drop extras onto map
                                timeSec_t curTime = Time::timeSec();
                                float stretch = cObj->slotTimeStretch;
                                
                                GridPos dropPos = 
                                    getPlayerPos( nextPlayer );
                            
                                // offset to counter-act offsets built into
                                // drop code
                                dropPos.x += 1;
                                dropPos.y += 1;

                                for( int s=newSlots; s<oldSlots; s++ ) {
                                    
                                    char found = false;
                                    GridPos spot;
                                
                                    if( getMapObject( dropPos.x, 
                                                      dropPos.y ) == 0 ) {
                                        spot = dropPos;
                                        found = true;
                                        }
                                    else {
                                        found = findDropSpot( 
                                            dropPos.x, dropPos.y,
                                            dropPos.x, dropPos.y,
                                            &spot );
                                        }
                            
                            
                                    if( found ) {
                                        setMapObject( 
                                            spot.x, spot.y,
                                            nextPlayer->
                                            clothingContained[c].
                                            getElementDirect( s ) );
                                        
                                        timeSec_t eta =
                                            nextPlayer->
                                            clothingContainedEtaDecays[c].
                                            getElementDirect( s );
                                        
                                        if( stretch != 1.0 ) {
                                            timeSec_t offset = 
                                                eta - curTime;
                    
                                            offset = offset / stretch;
                                            eta = curTime + offset;
                                            }
                                        
                                        setEtaDecay( spot.x, spot.y, eta );
                                        }
                                    }

                                nextPlayer->
                                    clothingContained[c].
                                    shrink( newSlots );
                                
                                nextPlayer->
                                    clothingContainedEtaDecays[c].
                                    shrink( newSlots );
                                }
                            
                            float oldStretch = 
                                cObj->slotTimeStretch;
                            float newStretch;
                            
                            if( newCObj != NULL ) {
                                newStretch = newCObj->slotTimeStretch;
                                }
                            else {
                                newStretch = oldStretch;
                                }
                            
                            if( oldStretch != newStretch ) {
                                timeSec_t curTime = Time::timeSec();
                                
                                for( int cc=0;
                                     cc < nextPlayer->
                                          clothingContainedEtaDecays[c].size();
                                     cc++ ) {
                                    
                                    if( nextPlayer->
                                        clothingContainedEtaDecays[c].
                                        getElementDirect( cc ) != 0 ) {
                                        
                                        timeSec_t offset = 
                                            nextPlayer->
                                            clothingContainedEtaDecays[c].
                                            getElementDirect( cc ) - 
                                            curTime;
                                        
                                        offset = offset * oldStretch;
                                        offset = offset / newStretch;
                                        
                                        *( nextPlayer->
                                           clothingContainedEtaDecays[c].
                                           getElement( cc ) ) =
                                            curTime + offset;
                                        }
                                    }
                                }

                            playerIndicesToSendUpdatesAbout.push_back( i );
                            }
                        else {
                            // no valid decay transition, end it
                            nextPlayer->clothingEtaDecay[c] = 0;
                            }
                        
                        }
                    
                    // check for decay of what's contained in clothing
                    if( cObj != NULL &&
                        nextPlayer->clothingContainedEtaDecays[c].size() > 0 ) {
                        
                        char change = false;
                        
                        SimpleVector<int> newContained;
                        SimpleVector<timeSec_t> newContainedETA;

                        for( int cc=0; 
                             cc <
                                 nextPlayer->
                                 clothingContainedEtaDecays[c].size();
                             cc++ ) {
                            
                            int oldID = nextPlayer->
                                clothingContained[c].getElementDirect( cc );
                            int newID = oldID;
                        
                            timeSec_t decay = 
                                nextPlayer->clothingContainedEtaDecays[c]
                                .getElementDirect( cc );

                            timeSec_t newDecay = decay;
                            
                            if( decay != 0 && decay < curTime ) {
                                
                                change = true;
                            
                                TransRecord *t = getPTrans( -1, oldID );
                                
                                newDecay = 0;

                                if( t != NULL ) {
                                    newID = t->newTarget;
                            
                                    if( newID != 0 ) {
                                        TransRecord *newDecayT = 
                                            getMetaTrans( -1, newID );
                                        
                                        if( newDecayT != NULL ) {
                                            newDecay = 
                                                Time::timeSec() +
                                                newDecayT->
                                                autoDecaySeconds /
                                                cObj->slotTimeStretch;
                                            }
                                        else {
                                            // no further decay
                                            newDecay = 0;
                                            }
                                        }
                                    }
                                }
                        
                            if( newID != 0 ) {
                                newContained.push_back( newID );
                                newContainedETA.push_back( newDecay );
                                } 
                            }
                        
                        if( change ) {
                            playerIndicesToSendUpdatesAbout.push_back( i );
                            
                            // assignment operator for vectors
                            // copies one vector into another
                            // replacing old contents
                            nextPlayer->clothingContained[c] =
                                newContained;
                            nextPlayer->clothingContainedEtaDecays[c] =
                                newContainedETA;
                            }
                        
                        }
                    
                    
                    }
                

                // check if they are done moving
                // if so, send an update
                

                if( nextPlayer->xd != nextPlayer->xs ||
                    nextPlayer->yd != nextPlayer->ys ) {
                
                    
                    // don't end new moves here (moves that 
                    // other players haven't been told about)
                    // even if they have come to an end time-wise
                    // wait until after we've told everyone about them
                    if( ! nextPlayer->newMove && 
                        Time::getCurrentTime() - nextPlayer->moveStartTime
                        >
                        nextPlayer->moveTotalSeconds ) {
                        
                        double moveSpeed = computeMoveSpeed( nextPlayer ) *
                            getPathSpeedModifier( nextPlayer->pathToDest,
                                                  nextPlayer->pathLength );


                        // done
                        nextPlayer->xs = nextPlayer->xd;
                        nextPlayer->ys = nextPlayer->yd;                        

                        printf( "Player %d's move is done at %d,%d\n",
                                nextPlayer->id,
                                nextPlayer->xs,
                                nextPlayer->ys );

                        if( nextPlayer->pathTruncated ) {
                            // truncated, but never told them about it
                            // force update now
                            nextPlayer->posForced = true;
                            }
                        playerIndicesToSendUpdatesAbout.push_back( i );

                        if( nextPlayer->holdingBiomeSickness ) {
                            int sicknessObjectID = 
                                getBiomeSickness( 
                                    nextPlayer->displayID, 
                                    nextPlayer->xs,
                                    nextPlayer->ys );
                            if( sicknessObjectID == -1 ) {
                                endBiomeSickness( 
                                    nextPlayer, i, 
                                    &playerIndicesToSendUpdatesAbout );
                                }
                            }

                        
                        // if they went far enough and fast enough
                        if( nextPlayer->holdingFlightObject &&
                            moveSpeed >= minFlightSpeed &&
                            ! nextPlayer->pathTruncated &&
                            nextPlayer->pathLength >= 2 ) {
                                    
                            // player takes off ?
                            
                            double xDir = 
                                nextPlayer->pathToDest[ 
                                      nextPlayer->pathLength - 1 ].x
                                  -
                                  nextPlayer->pathToDest[ 
                                      nextPlayer->pathLength - 2 ].x;
                            double yDir = 
                                nextPlayer->pathToDest[ 
                                      nextPlayer->pathLength - 1 ].y
                                  -
                                  nextPlayer->pathToDest[ 
                                      nextPlayer->pathLength - 2 ].y;
                            
                            int beyondEndX = nextPlayer->xs + xDir;
                            int beyondEndY = nextPlayer->ys + yDir;
                            
                            int endFloorID = getMapFloor( nextPlayer->xs,
                                                          nextPlayer->ys );
                            
                            int beyondEndFloorID = getMapFloor( beyondEndX,
                                                                beyondEndY );
                            
                            if( beyondEndFloorID != endFloorID ) {
                                // went all the way to the end of the 
                                // current floor in this direction, 
                                // take off there
                            
                                doublePair takeOffDir = { xDir, yDir };

                                int radiusLimit = -1;
                                
                                int barrierOn = SettingsManager::getIntSetting( 
                                    "barrierOn", 1 );
                                int barrierBlocksPlanes = 
                                    SettingsManager::getIntSetting( 
                                    "barrierBlocksPlanes", 1 );
                                
                                if( barrierOn && barrierBlocksPlanes ) {
                                    int barrierRadius = 
                                        SettingsManager::getIntSetting( 
                                            "barrierRadius", 250 );
                                    radiusLimit = barrierRadius;
                                    }

                                GridPos destPos = { -1, -1 };
                                
                                char foundMap = false;
                                if( Time::getCurrentTime() - 
                                    nextPlayer->forceFlightDestSetTime
                                    < 30 ) {
                                    // map fresh in memory

                                    
                                    destPos = getClosestLandingPos( 
                                        nextPlayer->forceFlightDest,
                                        &foundMap );
                                    
                                    // find strip closest to last
                                    // read map position
                                    AppLog::infoF( 
                                    "Player %d flight taking off from (%d,%d), "
                                    "map dest (%d,%d), found=%d, found (%d,%d)",
                                    nextPlayer->id,
                                    nextPlayer->xs, nextPlayer->ys,
                                    nextPlayer->forceFlightDest.x,
                                    nextPlayer->forceFlightDest.y,
                                    foundMap,
                                    destPos.x, destPos.y );
                                    }                                
                                if( ! foundMap ) {
                                    // find strip in flight direction
                                    
                                    destPos = getNextFlightLandingPos(
                                        nextPlayer->xs,
                                        nextPlayer->ys,
                                        takeOffDir,
                                        radiusLimit );
                                    
                                    AppLog::infoF( 
                                    "Player %d non-map flight taking off "
                                    "from (%d,%d), "
                                    "flightDir (%f,%f), dest (%d,%d)",
                                    nextPlayer->id,
                                    nextPlayer->xs, nextPlayer->ys,
                                    xDir, yDir,
                                    destPos.x, destPos.y );
                                    }
                                
                                
                                
                            
                                // send them a brand new map chunk
                                // around their new location
                                // and re-tell them about all players
                                // (relative to their new "birth" location...
                                //  see below)
                                nextPlayer->firstMessageSent = false;
                                nextPlayer->firstMapSent = false;
                                nextPlayer->inFlight = true;
                                
                                int destID = getMapObject( destPos.x,
                                                           destPos.y );
                                    
                                char heldTransHappened = false;
                                    
                                if( destID > 0 &&
                                    getObject( destID )->isFlightLanding ) {
                                    // found a landing place
                                    TransRecord *tr =
                                        getPTrans( nextPlayer->holdingID,
                                                   destID );
                                        
                                    if( tr != NULL ) {
                                        heldTransHappened = true;
                                            
                                        setMapObject( destPos.x, destPos.y,
                                                      tr->newTarget );

                                        transferHeldContainedToMap( 
                                            nextPlayer,
                                            destPos.x, destPos.y );

                                        handleHoldingChange(
                                            nextPlayer,
                                            tr->newActor );
                                            
                                        // stick player next to landing
                                        // pad
                                        destPos.x --;
                                        }
                                    }
                                if( ! heldTransHappened ) {
                                    // crash landing
                                    // force decay of held
                                    // no matter how much time is left
                                    // (flight uses fuel)
                                    TransRecord *decayTrans =
                                        getPTrans( -1, 
                                                   nextPlayer->holdingID );
                                        
                                    if( decayTrans != NULL ) {
                                        handleHoldingChange( 
                                            nextPlayer,
                                            decayTrans->newTarget );
                                        }
                                    }
                                    
                                FlightDest fd = {
                                    nextPlayer->id,
                                    destPos };

                                newFlightDest.push_back( fd );
                                
                                nextPlayer->xd = destPos.x;
                                nextPlayer->xs = destPos.x;
                                nextPlayer->yd = destPos.y;
                                nextPlayer->ys = destPos.y;

                                // reset their birth location
                                // their landing position becomes their
                                // new 0,0 for now
                                
                                // birth-relative coordinates enable the client
                                // (which is on a GPU with 32-bit floats)
                                // to operate at true coordinates well above
                                // the 23-bit preciions of 32-bit floats.
                                
                                // We keep the coordinates small by assuming
                                // that a player can never get too far from
                                // their birth location in one lifetime.
                                
                                // Flight teleportation violates this 
                                // assumption.
                                nextPlayer->birthPos.x = nextPlayer->xs;
                                nextPlayer->birthPos.y = nextPlayer->ys;
                                nextPlayer->heldOriginX = nextPlayer->xs;
                                nextPlayer->heldOriginY = nextPlayer->ys;
                                
                                nextPlayer->actionTarget.x = nextPlayer->xs;
                                nextPlayer->actionTarget.y = nextPlayer->ys;
                                }
                            }
                        }
                    }
                
                // check if we need to decrement their food
                double curTime = Time::getCurrentTime();
                
                if( ! nextPlayer->vogMode &&
                    curTime > 
                    nextPlayer->foodDecrementETASeconds ) {
                    
                    // only if femail of fertile age
                    char heldByFemale = false;
                    
                    if( nextPlayer->heldByOther ) {
                        LiveObject *adultO = getAdultHolding( nextPlayer );
                        
                        if( adultO != NULL &&
                            isFertileAge( adultO ) ) {
                    
                            heldByFemale = true;
                            }
                        }
                    
                    
                    LiveObject *decrementedPlayer = NULL;

                    if( !heldByFemale ) {

                        if( nextPlayer->yummyBonusStore > 0 ) {
                            nextPlayer->yummyBonusStore--;
                            }
                        else {
                            nextPlayer->foodStore--;
                            }
                        decrementedPlayer = nextPlayer;
                        }
                    // if held by fertile female, food for baby is free for
                    // duration of holding
                    
                    // only update the time of the fed player
                    nextPlayer->foodDecrementETASeconds = curTime +
                        computeFoodDecrementTimeSeconds( nextPlayer );

                    if( nextPlayer->drunkenness > 0 ) {
                        // for every unit of food consumed, consume one
                        // unit of drunkenness
                        nextPlayer->drunkenness -= 1.0;
                        if( nextPlayer->drunkenness < 0 ) {
                            nextPlayer->drunkenness = 0;
                            }
                        }
                    

                    if( decrementedPlayer != NULL &&
                        decrementedPlayer->foodStore < 0 ) {
                        // player has died
                        
                        // break the connection with them

                        if( heldByFemale ) {
                            setDeathReason( decrementedPlayer, 
                                            "nursing_hunger" );
                            }
                        else {
                            setDeathReason( decrementedPlayer, 
                                            "hunger" );
                            }
                        
                        decrementedPlayer->error = true;
                        decrementedPlayer->errorCauseString = "Player starved";


                        GridPos deathPos;
                                        
                        if( decrementedPlayer->xd == 
                            decrementedPlayer->xs &&
                            decrementedPlayer->yd ==
                            decrementedPlayer->ys ) {
                            // deleted player standing still
                            
                            deathPos.x = decrementedPlayer->xd;
                            deathPos.y = decrementedPlayer->yd;
                            }
                        else {
                            // player moving
                            
                            deathPos = 
                                computePartialMoveSpot( decrementedPlayer );
                            }
                        
                        if( ! decrementedPlayer->deathLogged &&
                            ! decrementedPlayer->isTutorial ) {    
                            logDeath( decrementedPlayer->id,
                                      decrementedPlayer->email,
                                      decrementedPlayer->isEve,
                                      computeAge( decrementedPlayer ),
                                      getSecondsPlayed( decrementedPlayer ),
                                      ! getFemale( decrementedPlayer ),
                                      deathPos.x, deathPos.y,
                                      players.size() - 1,
                                      false );
                            }
                        
                        if( shutdownMode &&
                            ! decrementedPlayer->isTutorial ) {
                            handleShutdownDeath( decrementedPlayer,
                                                 deathPos.x, deathPos.y );
                            }
                                            
                        decrementedPlayer->deathLogged = true;
                                        

                        // no negative
                        decrementedPlayer->foodStore = 0;
                        }
                    
                    if( decrementedPlayer != NULL ) {
                        decrementedPlayer->foodUpdate = true;
                        }
                    }
                
                }
            
            
            }
        

        
        // check for any that have been individually flagged, but
        // aren't on our list yet (updates caused by external triggers)
        for( int i=0; i<players.size() ; i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            
            if( nextPlayer->needsUpdate ) {
                playerIndicesToSendUpdatesAbout.push_back( i );
            
                nextPlayer->needsUpdate = false;
                }
            }
        

        // send updates about players who have had a posse-size change
        for( int i=0; i<killStatePosseChangedPlayerIDs.size(); i++ ) {
            int id = killStatePosseChangedPlayerIDs.getElementDirect( i );
            
            int index = getLiveObjectIndex( id );
            
            if( index != -1 ) {
                playerIndicesToSendUpdatesAbout.push_back( index );
                
                // end current move to allow move speed to change instantly
                endAnyMove( getLiveObject( id ) );
                }
            }
        
        killStatePosseChangedPlayerIDs.deleteAll();
        


        if( playerIndicesToSendUpdatesAbout.size() > 0 ) {
            
            SimpleVector<char> updateList;
        
            for( int i=0; i<playerIndicesToSendUpdatesAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendUpdatesAbout.getElementDirect( i ) );
                
                char *playerString = autoSprintf( "%d, ", nextPlayer->id );
                updateList.appendElementString( playerString );
                
                delete [] playerString;
                }
            
            char *updateListString = updateList.getElementString();
            
            AppLog::infoF( "Need to send updates about these %d players: %s",
                           playerIndicesToSendUpdatesAbout.size(),
                           updateListString );
            delete [] updateListString;
            }
        


        double currentTimeHeat = Time::getCurrentTime();
        
        if( currentTimeHeat - lastHeatUpdateTime >= heatUpdateTimeStep ) {
            // a heat step has passed
            
            
            // recompute heat map here for next players in line
            int r = 0;
            for( r=lastPlayerIndexHeatRecomputed+1; 
                 r < lastPlayerIndexHeatRecomputed + 1 + 
                     numPlayersRecomputeHeatPerStep
                     &&
                     r < players.size(); r++ ) {
                
                recomputeHeatMap( players.getElement( r ) );
                }
            
            lastPlayerIndexHeatRecomputed = r - 1;
            
            if( r >= players.size() ) {
                // done updating for last player
                // start over
                lastPlayerIndexHeatRecomputed = -1;
                }
            lastHeatUpdateTime = currentTimeHeat;
            }
        



        // update personal heat value of any player that is due
        // once every 2 seconds
        currentTime = Time::getCurrentTime();
        for( int i=0; i< players.size(); i++ ) {
            LiveObject *nextPlayer = players.getElement( i );
            
            if( nextPlayer->error ||
                currentTime - nextPlayer->lastHeatUpdate < heatUpdateSeconds ) {
                continue;
                }
            
            // in case we cross a biome boundary since last time
            // there will be thermal shock that will take them to
            // other side of target temp.
            // 
            // but never make them more comfortable (closer to
            // target) then they were before
            float oldDiffFromTarget = 
                targetHeat - nextPlayer->bodyHeat;


            if( nextPlayer->lastBiomeHeat != nextPlayer->biomeHeat ) {
                
          
                float lastBiomeDiffFromTarget = 
                    targetHeat - nextPlayer->lastBiomeHeat;
            
                float biomeDiffFromTarget = targetHeat - nextPlayer->biomeHeat;
            
                // for any biome
                // there's a "shock" when you enter it, if it's heat value
                // is on the other side of "perfect" from the temp you were at
                if( lastBiomeDiffFromTarget != 0 &&
                    biomeDiffFromTarget != 0 &&
                    sign( oldDiffFromTarget ) != 
                    sign( biomeDiffFromTarget ) ) {
                    
                    
                    // shock them to their mirror temperature on the meter
                    // (reflected across target temp)
                    nextPlayer->bodyHeat = targetHeat + oldDiffFromTarget;
                    }

                // we've handled this shock
                nextPlayer->lastBiomeHeat = nextPlayer->biomeHeat;
                }


            
            float clothingHeat = computeClothingHeat( nextPlayer );
            
            float heldHeat = computeHeldHeat( nextPlayer );
            

            float clothingR = computeClothingR( nextPlayer );

            // clothingR modulates heat lost (or gained) from environment
            float clothingLeak = 1 - clothingR;

            

            // what our body temp will move toward gradually
            // clothing heat and held heat are conductive
            // if they are present, they move envHeat up or down, before
            // we compute diff with body heat
            // (if they are 0, they have no effect)
            float envHeatTarget = clothingHeat + heldHeat + nextPlayer->envHeat;
            
            if( envHeatTarget < targetHeat ) {
                // we're in a cold environment

                if( nextPlayer->isIndoors ) {
                    float targetDiff = targetHeat - envHeatTarget;
                    float indoorAdjustedDiff = targetDiff / 2;
                    envHeatTarget = targetHeat - indoorAdjustedDiff;
                    }
                
                // clothing actually reduces how cold it is
                // based on its R-value

                // in other words, it "closes the gap" between our
                // perfect temp and our environmental temp

                // perfect clothing R would cut the environmental cold
                // factor in half

                float targetDiff = targetHeat - envHeatTarget;
                
                float clothingAdjustedDiff = targetDiff / ( 1 + clothingR );
                
                // how much did clothing improve our situation?
                float improvement = targetDiff - clothingAdjustedDiff;
                
                if( nextPlayer->isIndoors ) {
                    // if indoors, double the improvement of clothing
                    // thus, if it took us half-way to perfect, being
                    // indoors will take us all the way to perfect
                    // think about this as a reduction in the wind chill
                    // factor
                    
                    improvement *= 2;
                    }
                clothingAdjustedDiff = targetDiff - improvement;

                
                envHeatTarget = targetHeat - clothingAdjustedDiff;
                }
            

            // clothing only slows down temp movement AWAY from perfect
            if( abs( targetHeat - envHeatTarget ) <
                abs( targetHeat - nextPlayer->bodyHeat ) ) {
                // env heat is closer to perfect than our current body temp
                // clothing R should not apply in this case
                clothingLeak = 1.0;
                }
            
            
            float heatDelta = 
                clothingLeak * ( envHeatTarget 
                                 - 
                                 nextPlayer->bodyHeat );

            // slow this down a bit
            heatDelta *= 0.5;
            
            // feed through curve that is asymtotic at 1
            // (so we never change heat faster than 1 unit per timestep)
            
            float heatDeltaAbs = fabs( heatDelta );
            float heatDeltaSign = sign( heatDelta );

            float maxDelta = 2;
            // larger values make a sharper "knee"
            float deltaSlope = 0.5;
            
            // max - max/(slope*x+1)
            
            float heatDeltaScaled = 
                maxDelta - maxDelta / ( deltaSlope * heatDeltaAbs + 1 );
            
            heatDeltaScaled *= heatDeltaSign;


            nextPlayer->bodyHeat += heatDeltaScaled;
            
            // cap body heat, so that it doesn't climb way out of range
            // even in extreme situations
            if( nextPlayer->bodyHeat > 2 * targetHeat ) {
                nextPlayer->bodyHeat = 2 * targetHeat;
                }
            else if( nextPlayer->bodyHeat < 0 ) {
                nextPlayer->bodyHeat = 0;
                }
            
            
            float totalBodyHeat = nextPlayer->bodyHeat + nextPlayer->fever;
            
            // 0.25 body heat no longer added in each step above
            // add in a flat constant here to reproduce its effects
            // but only in a cold env (just like the old body heat)
            if( envHeatTarget < targetHeat ) {
                totalBodyHeat += 0.003;
                }



            // convert into 0..1 range, where 0.5 represents targetHeat
            nextPlayer->heat = ( totalBodyHeat / targetHeat ) / 2;
            if( nextPlayer->heat > 1 ) {
                nextPlayer->heat = 1;
                }
            if( nextPlayer->heat < 0 ) {
                nextPlayer->heat = 0;
                }

            nextPlayer->heatUpdate = true;
            nextPlayer->lastHeatUpdate = currentTime;
            }
        

        
        for( int i=0; i<playerIndicesToSendUpdatesAbout.size(); i++ ) {
            LiveObject *nextPlayer = players.getElement( 
                playerIndicesToSendUpdatesAbout.getElementDirect( i ) );

            if( nextPlayer->updateSent ) {
                continue;
                }
            
            
            if( nextPlayer->vogMode ) {
                // VOG players
                // handle this here, to take them out of circulation
                nextPlayer->updateSent = true;
                continue;
                }
            

            // also force-recompute heat maps for players that are getting
            // updated
            // don't bother with this for now
            // all players update on the same cycle
            // recomputeHeatMap( nextPlayer );
            
            
            
            newUpdates.push_back( getUpdateRecord( nextPlayer, false ) );
            
            newUpdatePlayerIDs.push_back( nextPlayer->id );
            

            if( nextPlayer->posForced &&
                nextPlayer->connected &&
                SettingsManager::getIntSetting( "requireClientForceAck", 1 ) ) {
                // block additional moves/actions from this player until
                // we get a FORCE response, syncing them up with
                // their forced position.
                
                // don't do this for disconnected players
                nextPlayer->waitingForForceResponse = true;
                }
            nextPlayer->posForced = false;


            ChangePosition p = { nextPlayer->xs, nextPlayer->ys, 
                                 nextPlayer->updateGlobal };
            newUpdatesPos.push_back( p );


            nextPlayer->updateSent = true;
            nextPlayer->updateGlobal = false;
            }
        
        

        if( newUpdates.size() > 0 ) {
            
            SimpleVector<char> trueUpdateList;
            
            
            for( int i=0; i<newUpdates.size(); i++ ) {
                char *s = autoSprintf( 
                    "%d, ", newUpdatePlayerIDs.getElementDirect( i ) );
                trueUpdateList.appendElementString( s );
                delete [] s;
                }
            
            char *updateListString = trueUpdateList.getElementString();
            
            AppLog::infoF( "Sending updates about these %d players: %s",
                           newUpdatePlayerIDs.size(),
                           updateListString );
            delete [] updateListString;
            }
        
        

        
        SimpleVector<ChangePosition> movesPos;        

        SimpleVector<MoveRecord> moveList = getMoveRecords( true, &movesPos );
        
        
                







        

        

        // add changes from auto-decays on map, 
        // mixed with player-caused changes
        stepMap( &mapChanges, &mapChangesPos );
        
        

        
        if( periodicStepThisStep ) {

            // figure out who has recieved a new curse token
            // they are sent a message about it below (CX)
            SimpleVector<char*> newCurseTokenEmails;
            getNewCurseTokenHolders( &newCurseTokenEmails );
        
            for( int i=0; i<newCurseTokenEmails.size(); i++ ) {
                char *email = newCurseTokenEmails.getElementDirect( i );
                
                for( int j=0; j<numLive; j++ ) {
                    LiveObject *nextPlayer = players.getElement(j);
                    
                    // don't give mid-life tokens to twins or cursed players
                    if( ! nextPlayer->isTwin &&
                        nextPlayer->curseStatus.curseLevel == 0 &&
                        strcmp( nextPlayer->email, email ) == 0 ) {
                        
                        nextPlayer->curseTokenCount = 1;
                        nextPlayer->curseTokenUpdate = true;
                        break;
                        }
                    }
                
                delete [] email;
                }
            }





        unsigned char *lineageMessage = NULL;
        int lineageMessageLength = 0;
        
        if( playerIndicesToSendLineageAbout.size() > 0 ) {
            SimpleVector<char> linWorking;
            linWorking.appendElementString( "LN\n" );
            
            int numAdded = 0;
            for( int i=0; i<playerIndicesToSendLineageAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendLineageAbout.getElementDirect( i ) );

                if( nextPlayer->error ) {
                    continue;
                    }
                getLineageLineForPlayer( nextPlayer, &linWorking );
                numAdded++;
                }
            
            linWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *lineageMessageText = linWorking.getElementString();
                
                lineageMessageLength = strlen( lineageMessageText );
                
                if( lineageMessageLength < maxUncompressedSize ) {
                    lineageMessage = (unsigned char*)lineageMessageText;
                    }
                else {
                    // compress for all players once here
                    lineageMessage = makeCompressedMessage( 
                        lineageMessageText, 
                        lineageMessageLength, &lineageMessageLength );
                    
                    delete [] lineageMessageText;
                    }
                }
            }




        unsigned char *cursesMessage = NULL;
        int cursesMessageLength = 0;
        
        if( playerIndicesToSendCursesAbout.size() > 0 ) {
            SimpleVector<char> curseWorking;
            curseWorking.appendElementString( "CU\n" );
            
            int numAdded = 0;
            for( int i=0; i<playerIndicesToSendCursesAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendCursesAbout.getElementDirect( i ) );

                if( nextPlayer->error ) {
                    continue;
                    }

                char *line = autoSprintf( "%d %d\n", nextPlayer->id,
                                         nextPlayer->curseStatus.curseLevel );
                
                curseWorking.appendElementString( line );
                delete [] line;
                numAdded++;
                }
            
            curseWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *cursesMessageText = curseWorking.getElementString();
                
                cursesMessageLength = strlen( cursesMessageText );
                
                if( cursesMessageLength < maxUncompressedSize ) {
                    cursesMessage = (unsigned char*)cursesMessageText;
                    }
                else {
                    // compress for all players once here
                    cursesMessage = makeCompressedMessage( 
                        cursesMessageText, 
                        cursesMessageLength, &cursesMessageLength );
                    
                    delete [] cursesMessageText;
                    }
                }
            }



        int followingMessageLength = 0;
        unsigned char *followingMessage = 
            getFollowingMessage( false, &followingMessageLength );
        

        int exileMessageLength = 0;
        unsigned char *exileMessage = 
            getExileMessage( false, &exileMessageLength );



        unsigned char *namesMessage = NULL;
        int namesMessageLength = 0;
        
        if( playerIndicesToSendNamesAbout.size() > 0 ) {
            SimpleVector<char> namesWorking;
            namesWorking.appendElementString( "NM\n" );
            
            int numAdded = 0;
            for( int i=0; i<playerIndicesToSendNamesAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendNamesAbout.getElementDirect( i ) );

                if( nextPlayer->error ) {
                    continue;
                    }

                char *line = autoSprintf( "%d %s\n", nextPlayer->id,
                                          nextPlayer->name );
                numAdded++;
                namesWorking.appendElementString( line );
                delete [] line;
                }
            
            namesWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *namesMessageText = namesWorking.getElementString();
                
                namesMessageLength = strlen( namesMessageText );
                
                if( namesMessageLength < maxUncompressedSize ) {
                    namesMessage = (unsigned char*)namesMessageText;
                    }
                else {
                    // compress for all players once here
                    namesMessage = makeCompressedMessage( 
                        namesMessageText, 
                        namesMessageLength, &namesMessageLength );
                    
                    delete [] namesMessageText;
                    }
                }
            }



        unsigned char *dyingMessage = NULL;
        int dyingMessageLength = 0;
        
        if( playerIndicesToSendDyingAbout.size() > 0 ) {
            SimpleVector<char> dyingWorking;
            dyingWorking.appendElementString( "DY\n" );
            
            int numAdded = 0;
            for( int i=0; i<playerIndicesToSendDyingAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendDyingAbout.getElementDirect( i ) );

                if( nextPlayer->error ) {
                    continue;
                    }
                
                char *line;
                
                if( nextPlayer->holdingEtaDecay > 0 ) {
                    // what they have will cure itself in time
                    // flag as sick
                    line = autoSprintf( "%d 1\n", nextPlayer->id );
                    }
                else {
                    line = autoSprintf( "%d\n", nextPlayer->id );
                    }
                
                numAdded++;
                dyingWorking.appendElementString( line );
                delete [] line;
                }
            
            dyingWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *dyingMessageText = dyingWorking.getElementString();
                
                dyingMessageLength = strlen( dyingMessageText );
                
                if( dyingMessageLength < maxUncompressedSize ) {
                    dyingMessage = (unsigned char*)dyingMessageText;
                    }
                else {
                    // compress for all players once here
                    dyingMessage = makeCompressedMessage( 
                        dyingMessageText, 
                        dyingMessageLength, &dyingMessageLength );
                    
                    delete [] dyingMessageText;
                    }
                }
            }




        unsigned char *healingMessage = NULL;
        int healingMessageLength = 0;
        
        if( playerIndicesToSendHealingAbout.size() > 0 ) {
            SimpleVector<char> healingWorking;
            healingWorking.appendElementString( "HE\n" );
            
            int numAdded = 0;
            for( int i=0; i<playerIndicesToSendHealingAbout.size(); i++ ) {
                LiveObject *nextPlayer = players.getElement( 
                    playerIndicesToSendHealingAbout.getElementDirect( i ) );

                if( nextPlayer->error ) {
                    continue;
                    }

                char *line = autoSprintf( "%d\n", nextPlayer->id );

                numAdded++;
                healingWorking.appendElementString( line );
                delete [] line;
                }
            
            healingWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *healingMessageText = healingWorking.getElementString();
                
                healingMessageLength = strlen( healingMessageText );
                
                if( healingMessageLength < maxUncompressedSize ) {
                    healingMessage = (unsigned char*)healingMessageText;
                    }
                else {
                    // compress for all players once here
                    healingMessage = makeCompressedMessage( 
                        healingMessageText, 
                        healingMessageLength, &healingMessageLength );
                    
                    delete [] healingMessageText;
                    }
                }
            }




        unsigned char *emotMessage = NULL;
        int emotMessageLength = 0;
        
        if( newEmotPlayerIDs.size() > 0 ) {
            SimpleVector<char> emotWorking;
            emotWorking.appendElementString( "PE\n" );
            
            int numAdded = 0;
            for( int i=0; i<newEmotPlayerIDs.size(); i++ ) {
                
                int ttl = newEmotTTLs.getElementDirect( i );
                int pID = newEmotPlayerIDs.getElementDirect( i );
                int eInd = newEmotIndices.getElementDirect( i );
                
                char *line;
                
                if( ttl == 0  ) {
                    line = autoSprintf( 
                        "%d %d\n", pID, eInd );
                    }
                else {
                    line = autoSprintf( 
                        "%d %d %d\n", pID, eInd, ttl );
                        
                    if( ttl == -1 ) {
                        // a new permanent emot
                        LiveObject *pO = getLiveObject( pID );
                        if( pO != NULL ) {
                            pO->permanentEmots.push_back( eInd );
                            }
                        }
                        
                    }
                
                numAdded++;
                emotWorking.appendElementString( line );
                delete [] line;
                }
            
            emotWorking.push_back( '#' );
            
            if( numAdded > 0 ) {

                char *emotMessageText = emotWorking.getElementString();
                
                emotMessageLength = strlen( emotMessageText );
                
                if( emotMessageLength < maxUncompressedSize ) {
                    emotMessage = (unsigned char*)emotMessageText;
                    }
                else {
                    // compress for all players once here
                    emotMessage = makeCompressedMessage( 
                        emotMessageText, 
                        emotMessageLength, &emotMessageLength );
                    
                    delete [] emotMessageText;
                    }
                }
            }

        
        SimpleVector<char*> newOwnerStrings;
        for( int u=0; u<newOwnerPos.size(); u++ ) {
            newOwnerStrings.push_back( 
                getOwnershipString( newOwnerPos.getElementDirect( u ) ) );
            }


        SimpleVector<char> babyWiggleLines;
        for( int i=0; i<players.size(); i++ ) {

            LiveObject *nextPlayer = players.getElement(i);
        
            if( nextPlayer->error ) {
                continue;
                }
            if( nextPlayer->wiggleUpdate ) {
                
                char *idString = autoSprintf( "%d\n", nextPlayer->id );
                babyWiggleLines.appendElementString( idString );
                delete [] idString;

                nextPlayer->wiggleUpdate = false;
                }
            }


        char *wiggleMessage = NULL;
        int wiggleMessageLength = 0;

        if( babyWiggleLines.size() > 0 ) {
            char *lines = babyWiggleLines.getElementString();
            
            wiggleMessage = autoSprintf( "BW\n%s#", lines );
            wiggleMessageLength = strlen( wiggleMessage );
            
            delete [] lines;
            }
        
        

        
        // send moves and updates to clients
        
        
        SimpleVector<int> playersReceivingPlayerUpdate;
        

        for( int i=0; i<numLive; i++ ) {
            
            LiveObject *nextPlayer = players.getElement(i);
            
            
            // everyone gets all flight messages
            // even if they haven't gotten first message yet
            // (because the flier will get their first message again
            // when they land, and we need to tell them about flight first)
            if( nextPlayer->firstMapSent ||
                nextPlayer->inFlight ) {
                                
                if( newFlightDest.size() > 0 ) {
                    
                    // compose FD messages for this player
                    
                    for( int u=0; u<newFlightDest.size(); u++ ) {
                        FlightDest *f = newFlightDest.getElement( u );
                        
                        char *flightMessage = 
                            autoSprintf( "FD\n%d %d %d\n#",
                                         f->playerID,
                                         f->destPos.x -
                                         nextPlayer->birthPos.x, 
                                         f->destPos.y -
                                         nextPlayer->birthPos.y );
                        
                        sendMessageToPlayer( nextPlayer, flightMessage,
                                             strlen( flightMessage ) );
                        delete [] flightMessage;
                        }
                    }
                }

            
            
            double maxDist = getMaxChunkDimension();
            double maxDist2 = maxDist * 2;

            
            if( ! nextPlayer->firstMessageSent ) {
                
                // send them their learned tool set
                // in case they are reconnecting and already know some tools
                sendLearnedToolMessage( nextPlayer, 
                                        &( nextPlayer->learnedTools ) );


                // first, send the map chunk around them
                
                int numSent = sendMapChunkMessage( nextPlayer );
                
                if( numSent == -2 ) {
                    // still not sent, try again later
                    continue;
                    }

                
                // next send info about valley lines

                int valleySpacing = 
                    SettingsManager::getIntSetting( "valleySpacing", 40 );
                                  
                char *valleyMessage = 
                    autoSprintf( "VS\n"
                                 "%d %d\n#",
                                 valleySpacing,
                                 nextPlayer->birthPos.y % valleySpacing );
                
                sendMessageToPlayer( nextPlayer, 
                                     valleyMessage, strlen( valleyMessage ) );
                
                delete [] valleyMessage;
                


                SimpleVector<int> outOfRangePlayerIDs;
                

                // now send starting message
                SimpleVector<char> messageBuffer;

                messageBuffer.appendElementString( "PU\n" );

                int numPlayers = players.size();
            
                // must be last in message
                char *playersLine = NULL;
                
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( ( o != nextPlayer && o->error ) 
                        ||
                        o->vogMode ) {
                        continue;
                        }

                    char oWasForced = o->posForced;
                    
                    if( nextPlayer->inFlight || 
                        nextPlayer->vogMode || nextPlayer->postVogMode ) {
                        // not a true first message
                        
                        // force all positions for all players
                        o->posForced = true;
                        }
                    

                    // true mid-move positions for first message
                    // all relative to new player's birth pos
                    char *messageLine = getUpdateLine( o, 
                                                       nextPlayer->birthPos,
                                                       getPlayerPos(
                                                           nextPlayer ),
                                                       false, true );
                    
                    if( nextPlayer->inFlight || 
                        nextPlayer->vogMode || nextPlayer->postVogMode ) {
                        // restore
                        o->posForced = oWasForced;
                        }
                    

                    // skip sending info about errored players in
                    // first message
                    if( o->id != nextPlayer->id ) {
                        messageBuffer.appendElementString( messageLine );
                        delete [] messageLine;
                        
                        double d = intDist( o->xd, o->yd, 
                                            nextPlayer->xd,
                                            nextPlayer->yd );
                        
                        if( d > maxDist ) {
                            outOfRangePlayerIDs.push_back( o->id );
                            }
                        }
                    else {
                        // save until end
                        playersLine = messageLine;
                        }
                    }
                
                if( playersLine != NULL ) {    
                    messageBuffer.appendElementString( playersLine );
                    delete [] playersLine;
                    }
                
                messageBuffer.push_back( '#' );
            
                char *message = messageBuffer.getElementString();


                sendMessageToPlayer( nextPlayer, message, strlen( message ) );
                
                delete [] message;


                // send out-of-range message for all players in PU above
                // that were out of range
                if( outOfRangePlayerIDs.size() > 0 ) {
                    SimpleVector<char> messageChars;
            
                    messageChars.appendElementString( "PO\n" );
            
                    for( int i=0; i<outOfRangePlayerIDs.size(); i++ ) {
                        char buffer[20];
                        sprintf( buffer, "%d\n",
                                 outOfRangePlayerIDs.getElementDirect( i ) );
                                
                        messageChars.appendElementString( buffer );
                        }
                    messageChars.push_back( '#' );

                    char *outOfRangeMessageText = 
                        messageChars.getElementString();
                    
                    sendMessageToPlayer( nextPlayer, outOfRangeMessageText,
                                         strlen( outOfRangeMessageText ) );

                    delete [] outOfRangeMessageText;
                    }
                
                

                char *movesMessage = 
                    getMovesMessage( false, 
                                     nextPlayer->birthPos,
                                     getPlayerPos( nextPlayer ) );
                
                if( movesMessage != NULL ) {
                    
                
                    sendMessageToPlayer( nextPlayer, movesMessage, 
                                         strlen( movesMessage ) );
                
                    delete [] movesMessage;
                    }



                // send lineage for everyone alive
                
                
                SimpleVector<char> linWorking;
                linWorking.appendElementString( "LN\n" );

                int numAdded = 0;
                
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( o->error ) {
                        continue;
                        }
                    
                    getLineageLineForPlayer( o, &linWorking );
                    numAdded++;
                    }
                
                linWorking.push_back( '#' );
            
                if( numAdded > 0 ) {
                    char *linMessage = linWorking.getElementString();


                    sendMessageToPlayer( nextPlayer, linMessage, 
                                         strlen( linMessage ) );
                
                    delete [] linMessage;
                    }



                // send names for everyone alive
                
                SimpleVector<char> namesWorking;
                namesWorking.appendElementString( "NM\n" );

                numAdded = 0;
                
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( o->error || o->name == NULL) {
                        continue;
                        }

                    char *line = autoSprintf( "%d %s\n", o->id, o->name );
                    namesWorking.appendElementString( line );
                    delete [] line;
                    
                    numAdded++;
                    }
                
                namesWorking.push_back( '#' );
            
                if( numAdded > 0 ) {
                    char *namesMessage = namesWorking.getElementString();


                    sendMessageToPlayer( nextPlayer, namesMessage, 
                                         strlen( namesMessage ) );
                
                    delete [] namesMessage;
                    }



                // send cursed status for all living cursed
                
                SimpleVector<char> cursesWorking;
                cursesWorking.appendElementString( "CU\n" );

                numAdded = 0;
                
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( o->error ) {
                        continue;
                        }

                    int level = o->curseStatus.curseLevel;
                    
                    if( level == 0 ) {
                        continue;
                        }
                    

                    char *line = autoSprintf( "%d %d\n", o->id, level );
                    cursesWorking.appendElementString( line );
                    delete [] line;
                    
                    numAdded++;
                    }
                
                cursesWorking.push_back( '#' );
            
                if( numAdded > 0 ) {
                    char *cursesMessage = cursesWorking.getElementString();


                    sendMessageToPlayer( nextPlayer, cursesMessage, 
                                         strlen( cursesMessage ) );
                
                    delete [] cursesMessage;
                    }
                

                if( nextPlayer->curseStatus.curseLevel > 0 ) {
                    // send player their personal report about how
                    // many excess curse points they have
                    
                    char *message = autoSprintf( 
                        "CS\n%d#", 
                        nextPlayer->curseStatus.excessPoints );

                    sendMessageToPlayer( nextPlayer, message, 
                                         strlen( message ) );
                
                    delete [] message;
                    }
                

                // send following status for everyone alive
                int followL = 0;
                unsigned char *followM = getFollowingMessage( true, &followL );
                
                if( followM != NULL ) {
                    nextPlayer->sock->send( 
                        followM, 
                        followL, 
                        false, false );
                    delete [] followM;
                    }



                // send exile status for everyone alive
                int exileL = 0;
                unsigned char *exileM = getExileMessage( true, &exileL );
                
                if( exileM != NULL ) {
                    nextPlayer->sock->send( 
                        exileM, 
                        exileL, 
                        false, false );
                    delete [] exileM;
                    }
                




                // send dying for everyone who is dying
                
                SimpleVector<char> dyingWorking;
                dyingWorking.appendElementString( "DY\n" );

                numAdded = 0;
                
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( o->error || ! o->dying ) {
                        continue;
                        }

                    char *line = autoSprintf( "%d\n", o->id );
                    dyingWorking.appendElementString( line );
                    delete [] line;
                    
                    numAdded++;
                    }
                
                dyingWorking.push_back( '#' );
            
                if( numAdded > 0 ) {
                    char *dyingMessage = dyingWorking.getElementString();


                    sendMessageToPlayer( nextPlayer, dyingMessage, 
                                         strlen( dyingMessage ) );
                
                    delete [] dyingMessage;
                    }
                

                // catch them up on war/peace states
                sendWarReportToOne( nextPlayer );
                
                if( ! nextPlayer->isTutorial &&
                    ! nextPlayer->forceSpawn ) {
                    // not skipping vog mode here, b/c it's never
                    // enabled until after first message sent
                    
                    // tell them about their own bad biomes
                    char *bbMessage = 
                        getBadBiomeMessage( nextPlayer->displayID );
                    sendMessageToPlayer( nextPlayer, bbMessage, 
                                         strlen( bbMessage ) );
                    
                    delete [] bbMessage;
                    }
                
                
                // tell them about all permanent emots
                SimpleVector<char> emotMessageWorking;
                emotMessageWorking.appendElementString( "PE\n" );
                for( int i=0; i<numPlayers; i++ ) {
                
                    LiveObject *o = players.getElement( i );
                
                    if( o->error ) {
                        continue;
                        }
                    for( int e=0; e< o->permanentEmots.size(); e ++ ) {
                        // ttl -2 for permanent but not new
                        char *line = autoSprintf( 
                            "%d %d -2\n",
                            o->id, 
                            o->permanentEmots.getElementDirect( e ) );
                        emotMessageWorking.appendElementString( line );
                        delete [] line;
                        }
                    }
                emotMessageWorking.push_back( '#' );
                
                char *emotMessage = emotMessageWorking.getElementString();
                
                sendMessageToPlayer( nextPlayer, emotMessage, 
                                     strlen( emotMessage ) );
                    
                delete [] emotMessage;
                    

                
                nextPlayer->firstMessageSent = true;
                nextPlayer->inFlight = false;
                nextPlayer->postVogMode = false;
                }
            else {
                // this player has first message, ready for updates/moves
                

                if( nextPlayer->monumentPosSet && 
                    ! nextPlayer->monumentPosSent &&
                    computeAge( nextPlayer ) > 0.5 ) {
                    
                    // they learned about a monument from their mother
                    
                    // wait until they are half a year old to tell them
                    // so they have a chance to load the sound first
                    
                    char *monMessage = 
                        autoSprintf( "MN\n%d %d %d\n#", 
                                     nextPlayer->lastMonumentPos.x -
                                     nextPlayer->birthPos.x, 
                                     nextPlayer->lastMonumentPos.y -
                                     nextPlayer->birthPos.y,
                                     nextPlayer->lastMonumentID );
                    
                    sendMessageToPlayer( nextPlayer, monMessage, 
                                         strlen( monMessage ) );
                    
                    nextPlayer->monumentPosSent = true;
                    
                    delete [] monMessage;
                    }




                // everyone gets all grave messages
                if( newGraves.size() > 0 ) {
                    
                    // compose GV messages for this player
                    
                    for( int u=0; u<newGraves.size(); u++ ) {
                        GraveInfo *g = newGraves.getElement( u );
                        
                        // only graves that are either in-range
                        // OR that are part of our family line.
                        // This prevents leaking relative positions
                        // through grave locations, but still allows
                        // us to return home after a long journey
                        // and find the grave of a family member
                        // who died while we were away.
                        if( distance( g->pos, getPlayerPos( nextPlayer ) )
                            < maxDist2 
                            ||
                            g->lineageEveID == nextPlayer->lineageEveID ) {
                            
                            char *graveMessage = 
                                autoSprintf( "GV\n%d %d %d\n#", 
                                             g->pos.x -
                                             nextPlayer->birthPos.x, 
                                             g->pos.y -
                                             nextPlayer->birthPos.y,
                                             g->playerID );
                            
                            sendMessageToPlayer( nextPlayer, graveMessage,
                                                 strlen( graveMessage ) );
                            delete [] graveMessage;
                            }
                        }
                    }


                // everyone gets all grave move messages
                if( newGraveMoves.size() > 0 ) {
                    
                    // compose GM messages for this player
                    
                    for( int u=0; u<newGraveMoves.size(); u++ ) {
                        GraveMoveInfo *g = newGraveMoves.getElement( u );
                        
                        // lineage info lost once grave moves
                        // and we still don't want long-distance relative
                        // position leaking happening here.
                        // So, far-away grave moves simply won't be 
                        // transmitted.  This may result in some confusion
                        // between different clients that have different
                        // info about graves, but that's okay.

                        // Anyway, if you're far from home, and your relative
                        // dies, you'll hear about the original grave.
                        // But then if someone moves the bones before you
                        // get home, you won't be able to find the grave
                        // by name after that.
                        
                        GridPos playerPos = getPlayerPos( nextPlayer );
                        
                        if( distance( g->posStart, playerPos )
                            < maxDist2 
                            ||
                            distance( g->posEnd, playerPos )
                            < maxDist2 ) {

                            char *graveMessage = 
                            autoSprintf( "GM\n%d %d %d %d %d\n#", 
                                         g->posStart.x -
                                         nextPlayer->birthPos.x,
                                         g->posStart.y -
                                         nextPlayer->birthPos.y,
                                         g->posEnd.x -
                                         nextPlayer->birthPos.x,
                                         g->posEnd.y -
                                         nextPlayer->birthPos.y,
                                         g->swapDest );
                        
                            sendMessageToPlayer( nextPlayer, graveMessage,
                                                 strlen( graveMessage ) );
                            delete [] graveMessage;
                            }
                        }
                    }
                
                
                // everyone gets all owner change messages
                if( newOwnerPos.size() > 0 ) {
                    
                    // compose OW messages for this player
                    for( int u=0; u<newOwnerPos.size(); u++ ) {
                        GridPos p = newOwnerPos.getElementDirect( u );
                        
                        // only pos that are either in-range
                        // OR are already known to this player.
                        // This prevents leaking relative positions
                        // through owned locations, but still allows
                        // us to instantly learn about important ownership
                        // changes
                        char known = isKnownOwned( nextPlayer, p );
                        
                        if( known ||
                            distance( p, getPlayerPos( nextPlayer ) )
                            < maxDist2 
                            ||
                            isOwned( nextPlayer, p ) ) {
                            
                            if( ! known ) {
                                // remember that we know about it now
                                nextPlayer->knownOwnedPositions.push_back( p );
                                }

                            char *ownerMessage = 
                                autoSprintf( 
                                    "OW\n%d %d%s\n#", 
                                    p.x -
                                    nextPlayer->birthPos.x, 
                                    p.y -
                                    nextPlayer->birthPos.y,
                                    newOwnerStrings.getElementDirect( u ) );
                            
                            sendMessageToPlayer( nextPlayer, ownerMessage,
                                                 strlen( ownerMessage ) );
                            delete [] ownerMessage;
                            }
                        }
                    }

                

                int playerXD = nextPlayer->xd;
                int playerYD = nextPlayer->yd;
                
                if( nextPlayer->heldByOther ) {
                    LiveObject *holdingPlayer = 
                        getLiveObject( nextPlayer->heldByOtherID );
                
                    if( holdingPlayer != NULL ) {
                        playerXD = holdingPlayer->xd;
                        playerYD = holdingPlayer->yd;
                        }
                    }


                if( abs( playerXD - nextPlayer->lastSentMapX ) > 7
                    ||
                    abs( playerYD - nextPlayer->lastSentMapY ) > 8 
                    ||
                    ! nextPlayer->firstMapSent ) {
                
                    // moving out of bounds of chunk, send update
                    // or player flagged as needing first map again
                    
                    sendMapChunkMessage( nextPlayer,
                                         // override if held
                                         nextPlayer->heldByOther,
                                         playerXD,
                                         playerYD );


                    // send updates about any non-moving players
                    // that are in this chunk
                    SimpleVector<char> chunkPlayerUpdates;

                    SimpleVector<char> chunkPlayerMoves;
                    

                    // add chunk updates for held babies first
                    for( int j=0; j<numLive; j++ ) {
                        LiveObject *otherPlayer = players.getElement( j );
                        
                        if( otherPlayer->error ) {
                            continue;
                            }


                        if( otherPlayer->heldByOther ) {
                            LiveObject *adultO = 
                                getAdultHolding( otherPlayer );
                            
                            if( adultO != NULL ) {
                                

                                if( adultO->id != nextPlayer->id &&
                                    otherPlayer->id != nextPlayer->id ) {
                                    // parent not us
                                    // baby not us

                                    double d = intDist( playerXD,
                                                        playerYD,
                                                        adultO->xd,
                                                        adultO->yd );
                            
                            
                                    if( d <= getMaxChunkDimension() / 2 ) {
                                        // adult holding this baby
                                        // is close enough
                                        // send update about baby
                                        char *updateLine = 
                                            getUpdateLine( otherPlayer,
                                                           nextPlayer->birthPos,
                                                           getPlayerPos( 
                                                               nextPlayer ),
                                                           false ); 
                                    
                                        chunkPlayerUpdates.
                                            appendElementString( updateLine );
                                        delete [] updateLine;
                                        }
                                    }
                                }
                            }
                        }
                    
                    
                    int ourHolderID = -1;
                    
                    if( nextPlayer->heldByOther ) {
                        LiveObject *adult = getAdultHolding( nextPlayer );
                        
                        if( adult != NULL ) {
                            ourHolderID = adult->id;
                            }
                        }
                    
                    // now send updates about all non-held babies,
                    // including any adults holding on-chunk babies
                    // here, AFTER we update about the babies

                    // (so their held status overrides the baby's stale
                    //  position status).
                    for( int j=0; j<numLive; j++ ) {
                        LiveObject *otherPlayer = 
                            players.getElement( j );
                        
                        if( otherPlayer->error ||
                            otherPlayer->vogMode ) {
                            continue;
                            }


                        if( !otherPlayer->heldByOther &&
                            otherPlayer->id != nextPlayer->id &&
                            otherPlayer->id != ourHolderID ) {
                            // not us
                            // not a held baby (covered above)
                            // no the adult holding us

                            double d = intDist( playerXD,
                                                playerYD,
                                                otherPlayer->xd,
                                                otherPlayer->yd );
                            
                            
                            if( d <= getMaxChunkDimension() / 2 ) {
                                
                                // send next player a player update
                                // about this player, telling nextPlayer
                                // where this player was last stationary
                                // and what they're holding

                                char *updateLine = 
                                    getUpdateLine( otherPlayer, 
                                                   nextPlayer->birthPos,
                                                   getPlayerPos( nextPlayer ),
                                                   false ); 
                                    
                                chunkPlayerUpdates.appendElementString( 
                                    updateLine );
                                delete [] updateLine;
                                

                                // We don't need to tell player about 
                                // moves in progress on this chunk.
                                // We're receiving move messages from 
                                // a radius of 32
                                // but this chunk has a radius of 16
                                // so we're hearing about player moves
                                // before they're on our chunk.
                                // Player moves have limited length,
                                // so there's no chance of a long move
                                // that started outside of our 32-radius
                                // finishinging inside this new chunk.
                                }
                            }
                        }


                    if( chunkPlayerUpdates.size() > 0 ) {
                        chunkPlayerUpdates.push_back( '#' );
                        char *temp = chunkPlayerUpdates.getElementString();

                        char *message = concatonate( "PU\n", temp );
                        delete [] temp;

                        sendMessageToPlayer( nextPlayer, message, 
                                             strlen( message ) );
                        
                        delete [] message;
                        }

                    
                    if( chunkPlayerMoves.size() > 0 ) {
                        char *temp = chunkPlayerMoves.getElementString();

                        sendMessageToPlayer( nextPlayer, temp, strlen( temp ) );

                        delete [] temp;
                        }
                    
                    // done handling sending new map chunk and player updates
                    // for players in the new chunk
                    }
                else {
                    // check if moving path goes near edge of player's
                    // known map
                    LiveObject *playerToCheck = nextPlayer;
                    if( nextPlayer->heldByOther ) {
                        LiveObject *holdingPlayer = 
                            getLiveObject( nextPlayer->heldByOtherID );
                        
                        if( holdingPlayer != NULL ) { 
                            playerToCheck = holdingPlayer;
                            }
                        }
                    
                    if( ( playerToCheck->xd != playerToCheck->xs ||
                          playerToCheck->yd != playerToCheck->ys ) 
                        && 
                        playerToCheck->pathToDest != NULL 
                        &&
                        ( nextPlayer->mapChunkPathCheckedDest.x 
                          != playerToCheck->xd || 
                          nextPlayer->mapChunkPathCheckedDest.y 
                          != playerToCheck->yd ) ) {
                        // moving and haven't checked this path before
                        // to see if it gets too close to the edge of the
                        // map
                        
                        // remember it to not check it again
                        nextPlayer->mapChunkPathCheckedDest.x =
                            playerToCheck->xd;
                        nextPlayer->mapChunkPathCheckedDest.y =
                            playerToCheck->yd;

                        // find most distant points on current path
                            
                        GridPos xFarPos, yFarPos;
                        int xFarPosDist = 0;
                        int yFarPosDist = 0;
                            
                        for( int i=0; i < playerToCheck->pathLength; i++ ) {
                            GridPos p = playerToCheck->pathToDest[i];
                                
                            int xdist = 
                                abs( p.x - nextPlayer->lastSentMapX );
                            int ydist = 
                                abs( p.y - nextPlayer->lastSentMapY );
                                
                            if( xdist > xFarPosDist ) {
                                xFarPos = p;
                                xFarPosDist = xdist;
                                }
                            if( ydist > yFarPosDist ) {
                                yFarPos = p;
                                yFarPosDist = ydist;
                                }
                            }
                            
                        if( xFarPosDist > 0 && 
                            abs( xFarPos.x - 
                                 nextPlayer->lastSentMapX ) > 7 ) {
                            
                            sendMapChunkMessage( nextPlayer,
                                                 // override chunk pos
                                                 true,
                                                 xFarPos.x,
                                                 xFarPos.y );
                            }
                        if( yFarPosDist > 0 &&
                            abs( yFarPos.y - 
                                 nextPlayer->lastSentMapY ) > 7 ) {
                                
                            sendMapChunkMessage( nextPlayer,
                                                 // override chunk pos
                                                 true,
                                                 yFarPos.x,
                                                 yFarPos.y );
                            }
                        }
                    }
                

                // EVERYONE gets info about dying players

                // do this first, so that PU messages about what they 
                // are holding post-wound come later                
                if( dyingMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            dyingMessage, 
                            dyingMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;

                    if( numSent != dyingMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }


                // EVERYONE gets info about now-healed players           
                if( healingMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            healingMessage, 
                            healingMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != healingMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }


                // EVERYONE gets info about emots           
                if( emotMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            emotMessage, 
                            emotMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != emotMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }
                

                // everyone gets wiggle message
                if( wiggleMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            (unsigned char*)wiggleMessage, 
                            wiggleMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != wiggleMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }
                

                
                // greater than maxDis but within maxDist2
                // for either PU or PM messages
                // (send PO for both, because we can have case
                // were a player coninously walks through the middleDistance
                // w/o ever stopping to create a PU message)
                SimpleVector<int> middleDistancePlayerIDs;
                



                if( newUpdates.size() > 0 && nextPlayer->connected ) {

                    double minUpdateDist = maxDist2 * 2;                    

                    for( int u=0; u<newUpdatesPos.size(); u++ ) {
                        ChangePosition *p = newUpdatesPos.getElement( u );
                        
                        // update messages can be global when a new
                        // player joins or an old player is deleted
                        if( p->global ) {
                            minUpdateDist = 0;
                            }
                        else {
                            double d = intDist( p->x, p->y, 
                                                playerXD, 
                                                playerYD );
                    
                            if( d < minUpdateDist ) {
                                minUpdateDist = d;
                                }
                            if( d > maxDist && d <= maxDist2 ) {
                                middleDistancePlayerIDs.push_back(
                                    newUpdatePlayerIDs.getElementDirect( u ) );
                                }
                            }
                        }

                    if( minUpdateDist <= maxDist ) {
                        // some updates close enough

                        // compose PU mesage for this player
                        
                        unsigned char *updateMessage = NULL;
                        int updateMessageLength = 0;
                        SimpleVector<char> updateChars;
                        
                        for( int u=0; u<newUpdates.size(); u++ ) {
                            ChangePosition *p = newUpdatesPos.getElement( u );
                        
                            double d = intDist( p->x, p->y, 
                                                playerXD, playerYD );
                            
                            if( ! p->global && d > maxDist ) {
                                // skip this one, too far away
                                continue;
                                }

                            if( p->global &&  d > maxDist ) {
                                // out of range global updates should
                                // also be followed by PO message
                                middleDistancePlayerIDs.push_back(
                                    newUpdatePlayerIDs.getElementDirect( u ) );
                                }
                            
                            
                            char *line =
                                getUpdateLineFromRecord( 
                                    newUpdates.getElement( u ),
                                    nextPlayer->birthPos,
                                    getPlayerPos( nextPlayer ) );
                            
                            updateChars.appendElementString( line );
                            delete [] line;
                            }
                        

                        if( updateChars.size() > 0 ) {
                            updateChars.push_back( '#' );
                            char *temp = updateChars.getElementString();

                            char *updateMessageText = 
                                concatonate( "PU\n", temp );
                            delete [] temp;
                            
                            updateMessageLength = strlen( updateMessageText );

                            if( updateMessageLength < maxUncompressedSize ) {
                                updateMessage = 
                                    (unsigned char*)updateMessageText;
                                }
                            else {
                                updateMessage = makeCompressedMessage( 
                                    updateMessageText, 
                                    updateMessageLength, &updateMessageLength );
                
                                delete [] updateMessageText;
                                }
                            }

                        if( updateMessage != NULL ) {
                            playersReceivingPlayerUpdate.push_back( 
                                nextPlayer->id );
                            
                            int numSent = 
                                nextPlayer->sock->send( 
                                    updateMessage, 
                                    updateMessageLength, 
                                    false, false );
                            
                            nextPlayer->gotPartOfThisFrame = true;
                            
                            delete [] updateMessage;
                            
                            if( numSent != updateMessageLength ) {
                                setPlayerDisconnected( nextPlayer, 
                                                       "Socket write failed" );
                                }
                            }
                        }
                    }




                if( moveList.size() > 0 && nextPlayer->connected ) {
                    
                    double minUpdateDist = getMaxChunkDimension() * 2;
                    
                    for( int u=0; u<movesPos.size(); u++ ) {
                        ChangePosition *p = movesPos.getElement( u );
                        
                        // move messages are never global

                        double d = intDist( p->x, p->y, 
                                            playerXD, playerYD );
                    
                        if( d < minUpdateDist ) {
                            minUpdateDist = d;
                            }
                        if( d > maxDist && d <= maxDist2 ) {
                            middleDistancePlayerIDs.push_back(
                                moveList.getElement( u )->playerID );
                            }
                        }

                    if( minUpdateDist <= maxDist ) {
                        
                        SimpleVector<MoveRecord> closeMoves;
                        
                        for( int u=0; u<movesPos.size(); u++ ) {
                            ChangePosition *p = movesPos.getElement( u );
                            
                            // move messages are never global
                            
                            double d = intDist( p->x, p->y, 
                                                playerXD, playerYD );
                    
                            if( d > maxDist ) {
                                continue;
                                }
                            closeMoves.push_back( 
                                moveList.getElementDirect( u ) );
                            }
                        
                        if( closeMoves.size() > 0 ) {
                            
                            char *moveMessageText = getMovesMessageFromList( 
                                &closeMoves, nextPlayer->birthPos );
                        
                            unsigned char *moveMessage = NULL;
                            int moveMessageLength = 0;
        
                            if( moveMessageText != NULL ) {
                                moveMessage = (unsigned char*)moveMessageText;
                                moveMessageLength = strlen( moveMessageText );

                                if( moveMessageLength > maxUncompressedSize ) {
                                    moveMessage = makeCompressedMessage( 
                                        moveMessageText,
                                        moveMessageLength,
                                        &moveMessageLength );
                                    delete [] moveMessageText;
                                    }    
                                }

                            int numSent = 
                                nextPlayer->sock->send( 
                                    moveMessage, 
                                    moveMessageLength, 
                                    false, false );
                            
                            nextPlayer->gotPartOfThisFrame = true;
                            
                            delete [] moveMessage;
                            
                            if( numSent != moveMessageLength ) {
                                setPlayerDisconnected( nextPlayer, 
                                                       "Socket write failed" );
                                }
                            }
                        }
                    }
                

                
                // now send PO for players that are out of range
                // who are moving or updating above
                if( middleDistancePlayerIDs.size() > 0 
                    && nextPlayer->connected ) {
                    
                    unsigned char *outOfRangeMessage = NULL;
                    int outOfRangeMessageLength = 0;
                    
                    if( middleDistancePlayerIDs.size() > 0 ) {
                        SimpleVector<char> messageChars;
            
                        messageChars.appendElementString( "PO\n" );
            
                        for( int i=0; 
                             i<middleDistancePlayerIDs.size(); i++ ) {
                            char buffer[20];
                            sprintf( 
                                buffer, "%d\n",
                                middleDistancePlayerIDs.
                                getElementDirect( i ) );
                                
                            messageChars.appendElementString( buffer );
                            }
                        messageChars.push_back( '#' );

                        char *outOfRangeMessageText = 
                            messageChars.getElementString();

                        outOfRangeMessageLength = 
                            strlen( outOfRangeMessageText );

                        if( outOfRangeMessageLength < 
                            maxUncompressedSize ) {
                            outOfRangeMessage = 
                                (unsigned char*)outOfRangeMessageText;
                            }
                        else {
                            // compress 
                            outOfRangeMessage = makeCompressedMessage( 
                                outOfRangeMessageText, 
                                outOfRangeMessageLength, 
                                &outOfRangeMessageLength );
                
                            delete [] outOfRangeMessageText;
                            }
                        }
                        
                    int numSent = 
                        nextPlayer->sock->send( 
                            outOfRangeMessage, 
                            outOfRangeMessageLength, 
                            false, false );
                        
                    nextPlayer->gotPartOfThisFrame = true;

                    delete [] outOfRangeMessage;

                    if( numSent != outOfRangeMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }



                
                if( mapChanges.size() > 0 && nextPlayer->connected ) {
                    double minUpdateDist = getMaxChunkDimension() * 2;
                    
                    for( int u=0; u<mapChangesPos.size(); u++ ) {
                        ChangePosition *p = mapChangesPos.getElement( u );
                        
                        // map changes are never global

                        double d = intDist( p->x, p->y, 
                                            playerXD, playerYD );
                        
                        if( d < minUpdateDist ) {
                            minUpdateDist = d;
                            }
                        }

                    if( minUpdateDist <= maxDist ) {
                        // at least one thing in map change list is close
                        // enough to this player

                        // format custom map change message for this player
                        
                        
                        unsigned char *mapChangeMessage = NULL;
                        int mapChangeMessageLength = 0;
                        SimpleVector<char> mapChangeChars;

                        for( int u=0; u<mapChanges.size(); u++ ) {
                            ChangePosition *p = mapChangesPos.getElement( u );
                        
                            double d = intDist( p->x, p->y, 
                                                playerXD, playerYD );
                            
                            if( d > maxDist ) {
                                // skip this one, too far away
                                continue;
                                }
                            MapChangeRecord *r = 
                                mapChanges.getElement( u );
                            
                            char *lineString =
                                getMapChangeLineString( 
                                    r,
                                    nextPlayer->birthPos.x,
                                    nextPlayer->birthPos.y );
                            
                            mapChangeChars.appendElementString( lineString );
                            delete [] lineString;
                            }
                        
                        
                        if( mapChangeChars.size() > 0 ) {
                            mapChangeChars.push_back( '#' );
                            char *temp = mapChangeChars.getElementString();

                            char *mapChangeMessageText = 
                                concatonate( "MX\n", temp );
                            delete [] temp;

                            mapChangeMessageLength = 
                                strlen( mapChangeMessageText );
            
                            if( mapChangeMessageLength < 
                                maxUncompressedSize ) {
                                mapChangeMessage = 
                                    (unsigned char*)mapChangeMessageText;
                                }
                            else {
                                mapChangeMessage = makeCompressedMessage( 
                                    mapChangeMessageText, 
                                    mapChangeMessageLength, 
                                    &mapChangeMessageLength );
                
                                delete [] mapChangeMessageText;
                                }
                            }

                        
                        if( mapChangeMessage != NULL ) {

                            int numSent = 
                                nextPlayer->sock->send( 
                                    mapChangeMessage, 
                                    mapChangeMessageLength, 
                                    false, false );
                            
                            nextPlayer->gotPartOfThisFrame = true;
                            
                            delete [] mapChangeMessage;

                            if( numSent != mapChangeMessageLength ) {
                                setPlayerDisconnected( nextPlayer, 
                                                       "Socket write failed" );
                                }
                            }
                        }
                    }
                if( newSpeechPos.size() > 0 && nextPlayer->connected ) {
                    double minUpdateDist = maxSpeechRadius * 2;
                    
                    for( int u=0; u<newSpeechPos.size(); u++ ) {
                        ChangePosition *p = newSpeechPos.getElement( u );
                        
                        // speech never global

                        double d = intDist( p->x, p->y, 
                                            playerXD, playerYD );
                        
                        if( d < minUpdateDist ) {
                            minUpdateDist = d;
                            }
                        }

                    if( minUpdateDist <= maxSpeechRadius ) {

                        SimpleVector<char> messageWorking;
                        messageWorking.appendElementString( "PS\n" );
                        
                        
                        for( int u=0; u<newSpeechPos.size(); u++ ) {

                            ChangePosition *p = newSpeechPos.getElement( u );
                        
                            // speech never global
                            
                            double d = intDist( p->x, p->y, 
                                                playerXD, playerYD );
                            
                            if( d <= maxSpeechRadius ) {

                                int speakerID = 
                                    newSpeechPlayerIDs.getElementDirect( u );
                                LiveObject *speakerObj =
                                    getLiveObject( speakerID );
                                
                                int listenerEveID = nextPlayer->lineageEveID;
                                int listenerID = nextPlayer->id;
                                double listenerAge = computeAge( nextPlayer );
                                int listenerParentID = nextPlayer->parentID;
                                
                                int speakerEveID;
                                double speakerAge;
                                int speakerParentID = -1;
                                
                                if( speakerObj != NULL ) {
                                    speakerEveID = speakerObj->lineageEveID;
                                    speakerID = speakerObj->id;
                                    speakerAge = computeAge( speakerObj );
                                    speakerParentID = speakerObj->parentID;
                                    }
                                else {
                                    // speaker dead, doesn't matter what we
                                    // do
                                    speakerEveID = listenerEveID;
                                    speakerID = listenerID;
                                    speakerAge = listenerAge;
                                    }
                                

                                char *trimmedPhrase =
                                    stringDuplicate( newSpeechPhrases.
                                                     getElementDirect( u ) );

                                char *starLoc = 
                                    strstr( trimmedPhrase, " *map" );
                                    
                                if( starLoc != NULL ) {
                                    if( speakerID != listenerID ) {
                                        // only send map metadata through
                                        // if we picked up the map ourselves
                                        // trim it otherwise
                                        
                                        starLoc[0] = '\0';
                                        }
                                    else {
                                        // make coords birth-relative
                                        // to person reading map
                                        int mapX, mapY;
                                        
                                        int numRead = 
                                            sscanf( starLoc, 
                                                    " *map %d %d",
                                                    &mapX, &mapY );
                                        if( numRead == 2 ) {
                                            starLoc[0] = '\0';
                                            char *newTrimmed = autoSprintf( 
                                                "%s *map %d %d",
                                                trimmedPhrase,
                                                mapX - nextPlayer->birthPos.x, 
                                                mapY - nextPlayer->birthPos.y );
                                            
                                            delete [] trimmedPhrase;
                                            trimmedPhrase = newTrimmed;

                                            if( speakerObj != NULL ) {
                                                speakerObj->forceFlightDest.x
                                                    = mapX;
                                                speakerObj->forceFlightDest.y
                                                    = mapY;
                                                speakerObj->
                                                    forceFlightDestSetTime
                                                    = Time::getCurrentTime();
                                                }
                                            }
                                        }
                                    }

                                
                                char *translatedPhrase;
                                
                                // skip language filtering in some cases
                                // VOG can talk to anyone
                                // so can force spawns
                                // also, skip in on very low pop servers
                                // (just let everyone talk together)
                                // also in case where speach is server-forced
                                // sound representations (like [GASP])
                                // but NOT for reading written words
                                if( nextPlayer->vogMode || 
                                    nextPlayer->forceSpawn || 
                                    ( speakerObj != NULL &&
                                      speakerObj->vogMode ) ||
                                    ( speakerObj != NULL &&
                                      speakerObj->forceSpawn ) ||
                                    players.size() < 
                                    minActivePlayersForLanguages ||
                                    strlen( trimmedPhrase ) == 0 ||
                                    trimmedPhrase[0] == '[' ||
                                    isPolylingual( nextPlayer->displayID ) ||
                                    ( speakerObj != NULL &&
                                      isPolylingual( 
                                          speakerObj->displayID ) ) ) {
                                    
                                    translatedPhrase =
                                        stringDuplicate( trimmedPhrase );
                                    }
                                else {
                                    int speakerDrunkenness = 0;
                                    
                                    if( speakerObj != NULL ) {
                                        speakerDrunkenness =
                                            speakerObj->drunkenness;
                                        }

                                    translatedPhrase =
                                        mapLanguagePhrase( 
                                            trimmedPhrase,
                                            speakerEveID,
                                            listenerEveID,
                                            speakerID,
                                            listenerID,
                                            speakerAge,
                                            listenerAge,
                                            speakerParentID,
                                            listenerParentID,
                                            speakerDrunkenness / 10.0 );
                                    }
                                
                                if( speakerEveID != 
                                    listenerEveID
                                    && speakerAge > 55 
                                    && listenerAge > 55 ) {
                                    
                                    if( strcmp( translatedPhrase, "PEACE" )
                                        == 0 ) {
                                        // an elder speaker
                                        // said PEACE 
                                        // in elder listener's language
                                        addPeaceTreaty( speakerEveID,
                                                        listenerEveID );
                                        }
                                    else if( strcmp( translatedPhrase, 
                                                     "WAR" )
                                             == 0 ) {
                                            // an elder speaker
                                        // said WAR 
                                        // in elder listener's language
                                        removePeaceTreaty( speakerEveID,
                                                           listenerEveID );
                                        }
                                    }
                                
                                if( speakerObj != NULL &&
                                    speakerObj->drunkenness > 0 ) {
                                    // slur their speech
                                    
                                    char *slurredPhrase =
                                        slurSpeech( speakerObj->id,
                                                    translatedPhrase,
                                                    speakerObj->drunkenness );
                                    
                                    delete [] translatedPhrase;
                                    translatedPhrase = slurredPhrase;
                                    }
                                

                                int curseFlag =
                                    newSpeechCurseFlags.getElementDirect( u );

                                char *line = autoSprintf( "%d/%d %s\n", 
                                                          speakerID,
                                                          curseFlag,
                                                          translatedPhrase );
                                delete [] translatedPhrase;
                                delete [] trimmedPhrase;
                                
                                messageWorking.appendElementString( line );
                                
                                delete [] line;
                                }
                            }
                        
                        messageWorking.appendElementString( "#" );
                            
                        char *messageText = 
                            messageWorking.getElementString();
                        
                        int messageLen = strlen( messageText );
                        
                        unsigned char *message = 
                            (unsigned char*) messageText;
                        
                        
                        if( messageLen >= maxUncompressedSize ) {
                            char *old = messageText;
                            int oldLen = messageLen;
                            
                            message = makeCompressedMessage( 
                                old, 
                                oldLen, &messageLen );
                            
                            delete [] old;
                            }
                        
                        
                        int numSent = 
                            nextPlayer->sock->send( 
                                message, 
                                messageLen, 
                                false, false );
                        
                        delete [] message;
                        
                        nextPlayer->gotPartOfThisFrame = true;
                        
                        if( numSent != messageLen ) {
                            setPlayerDisconnected( nextPlayer, 
                                                   "Socket write failed" );
                            }
                        }
                    }


                if( newLocationSpeech.size() > 0 && nextPlayer->connected ) {
                    double minUpdateDist = maxSpeechRadius * 2;
                    
                    for( int u=0; u<newLocationSpeechPos.size(); u++ ) {
                        ChangePosition *p = 
                            newLocationSpeechPos.getElement( u );
                        
                        // locationSpeech never global

                        double d = intDist( p->x, p->y, 
                                            playerXD, playerYD );
                        
                        if( d < minUpdateDist ) {
                            minUpdateDist = d;
                            }
                        }

                    if( minUpdateDist <= maxSpeechRadius ) {
                        // some of location speech in range
                        
                        SimpleVector<char> working;
                        
                        working.appendElementString( "LS\n" );
                        
                        for( int u=0; u<newLocationSpeechPos.size(); u++ ) {
                            ChangePosition *p = 
                                newLocationSpeechPos.getElement( u );
                            
                            double d = intDist( p->x, p->y, 
                                                playerXD, playerYD );
                            
                            if( d <= maxSpeechRadius ) {

                                char *line = autoSprintf( 
                                    "%d %d %s\n",
                                    p->x - nextPlayer->birthPos.x, 
                                    p->y - nextPlayer->birthPos.y,
                                    newLocationSpeech.getElementDirect( u ) );
                                working.appendElementString( line );
                                
                                delete [] line;
                                }
                            }
                        working.push_back( '#' );
                        
                        char *message = 
                            working.getElementString();
                        int len = working.size();
                        

                        if( len > maxUncompressedSize ) {
                            int compLen = 0;
                            
                            unsigned char *compMessage = makeCompressedMessage( 
                                message, 
                                len, 
                                &compLen );
                
                            delete [] message;
                            len = compLen;
                            message = (char*)compMessage;
                            }

                        int numSent = 
                            nextPlayer->sock->send( 
                                (unsigned char*)message,
                                len, 
                                false, false );
                        
                        delete [] message;
                        
                        nextPlayer->gotPartOfThisFrame = true;
                        
                        if( numSent != len ) {
                            setPlayerDisconnected( nextPlayer, 
                                                   "Socket write failed" );
                            }
                        }
                    }
                


                // EVERYONE gets updates about deleted players                
                if( nextPlayer->connected ) {
                    
                    unsigned char *deleteUpdateMessage = NULL;
                    int deleteUpdateMessageLength = 0;
        
                    SimpleVector<char> deleteUpdateChars;
                
                    for( int u=0; u<newDeleteUpdates.size(); u++ ) {
                    
                        char *line = getUpdateLineFromRecord(
                            newDeleteUpdates.getElement( u ),
                            nextPlayer->birthPos,
                            getPlayerPos( nextPlayer ) );
                    
                        deleteUpdateChars.appendElementString( line );
                    
                        delete [] line;
                        }
                

                    if( deleteUpdateChars.size() > 0 ) {
                        deleteUpdateChars.push_back( '#' );
                        char *temp = deleteUpdateChars.getElementString();
                    
                        char *deleteUpdateMessageText = 
                            concatonate( "PU\n", temp );
                        delete [] temp;
                    
                        deleteUpdateMessageLength = 
                            strlen( deleteUpdateMessageText );

                        if( deleteUpdateMessageLength < maxUncompressedSize ) {
                            deleteUpdateMessage = 
                                (unsigned char*)deleteUpdateMessageText;
                            }
                        else {
                            // compress for all players once here
                            deleteUpdateMessage = makeCompressedMessage( 
                                deleteUpdateMessageText, 
                                deleteUpdateMessageLength, 
                                &deleteUpdateMessageLength );
                
                            delete [] deleteUpdateMessageText;
                            }
                        }

                    if( deleteUpdateMessage != NULL ) {
                        int numSent = 
                            nextPlayer->sock->send( 
                                deleteUpdateMessage, 
                                deleteUpdateMessageLength, 
                                false, false );
                    
                        nextPlayer->gotPartOfThisFrame = true;
                    
                        delete [] deleteUpdateMessage;
                    
                        if( numSent != deleteUpdateMessageLength ) {
                            setPlayerDisconnected( nextPlayer, 
                                                   "Socket write failed" );
                            }
                        }
                    }



                // EVERYONE gets lineage info for new babies
                if( lineageMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            lineageMessage, 
                            lineageMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != lineageMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }


                // EVERYONE gets curse info
                if( cursesMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            cursesMessage, 
                            cursesMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != cursesMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }

                // EVERYONE gets newly-given names
                if( namesMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            namesMessage, 
                            namesMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != namesMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }


                // EVERYONE gets following message
                if( followingMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            followingMessage, 
                            followingMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != followingMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }


                // EVERYONE gets exile message
                if( exileMessage != NULL && nextPlayer->connected ) {
                    int numSent = 
                        nextPlayer->sock->send( 
                            exileMessage, 
                            exileMessageLength, 
                            false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != exileMessageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    }

                


                if( nextPlayer->foodUpdate ) {
                    // send this player a food status change
                    
                    int cap = computeFoodCapacity( nextPlayer );
                    
                    if( cap < nextPlayer->foodStore ) {
                        nextPlayer->foodStore = cap;
                        }
                    
                    if( cap > nextPlayer->lastReportedFoodCapacity ) {
                        
                        // stomach grew
                        
                        // fill empty space from bonus store automatically
                        int extraCap = 
                            cap - nextPlayer->lastReportedFoodCapacity;
                        
                        while( nextPlayer->yummyBonusStore > 0 && 
                               extraCap > 0 &&
                               nextPlayer->foodStore < cap ) {
                            nextPlayer->foodStore ++;
                            extraCap --;
                            nextPlayer->yummyBonusStore--;
                            }
                        }
                    

                    nextPlayer->lastReportedFoodCapacity = cap;
                    

                    int yumMult = nextPlayer->yummyFoodChain.size() - 1;
                    
                    if( yumMult < 0 ) {
                        yumMult = 0;
                        }
                    
                    if( nextPlayer->connected ) {
                        
                        char *foodMessage = autoSprintf( 
                            "FX\n"
                            "%d %d %d %d %.2f %d "
                            "%d %d\n"
                            "#",
                            nextPlayer->foodStore,
                            cap,
                            hideIDForClient( nextPlayer->lastAteID ),
                            nextPlayer->lastAteFillMax,
                            computeMoveSpeed( nextPlayer ),
                            nextPlayer->responsiblePlayerID,
                            nextPlayer->yummyBonusStore,
                            yumMult );
                        
                        int messageLength = strlen( foodMessage );
                        
                        int numSent = 
                            nextPlayer->sock->send( 
                                (unsigned char*)foodMessage, 
                                messageLength,
                                false, false );
                        
                        nextPlayer->gotPartOfThisFrame = true;
                        
                        if( numSent != messageLength ) {
                            setPlayerDisconnected( nextPlayer, 
                                                   "Socket write failed" );
                            }
                        
                        delete [] foodMessage;
                        }
                    
                    nextPlayer->foodUpdate = false;
                    nextPlayer->lastAteID = 0;
                    nextPlayer->lastAteFillMax = 0;
                    }



                if( nextPlayer->heatUpdate && nextPlayer->connected ) {
                    // send this player a heat status change
                    
                    // recompute now to update their decrement time
                    // and indoor bonus for this message
                    computeFoodDecrementTimeSeconds( nextPlayer );
                    
                    char *heatMessage = autoSprintf( 
                        "HX\n"
                        "%.2f %.2f %.2f#",
                        nextPlayer->heat,
                        nextPlayer->foodDrainTime,
                        nextPlayer->indoorBonusTime );
                     
                    int messageLength = strlen( heatMessage );
                    
                    int numSent = 
                         nextPlayer->sock->send( 
                             (unsigned char*)heatMessage, 
                             messageLength,
                             false, false );
                    
                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != messageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    
                    delete [] heatMessage;
                    }
                nextPlayer->heatUpdate = false;
                    

                if( nextPlayer->curseTokenUpdate &&
                    nextPlayer->connected ) {
                    // send this player a curse token status change
                    
                    char *tokenMessage = autoSprintf( 
                        "CX\n"
                        "%d#",
                        nextPlayer->curseTokenCount );
                     
                    int messageLength = strlen( tokenMessage );
                    
                    int numSent = 
                         nextPlayer->sock->send( 
                             (unsigned char*)tokenMessage, 
                             messageLength,
                             false, false );

                    nextPlayer->gotPartOfThisFrame = true;
                    
                    if( numSent != messageLength ) {
                        setPlayerDisconnected( nextPlayer, 
                                               "Socket write failed" );
                        }
                    
                    delete [] tokenMessage;                    
                    }
                nextPlayer->curseTokenUpdate = false;

                }
            }


        for( int u=0; u<moveList.size(); u++ ) {
            MoveRecord *r = moveList.getElement( u );
            delete [] r->formatString;
            }



        for( int u=0; u<mapChanges.size(); u++ ) {
            MapChangeRecord *r = mapChanges.getElement( u );
            delete [] r->formatString;
            }

        if( newUpdates.size() > 0 ) {
            
            SimpleVector<char> playerList;
            
            for( int i=0; i<playersReceivingPlayerUpdate.size(); i++ ) {
                char *playerString = 
                    autoSprintf( 
                        "%d, ",
                        playersReceivingPlayerUpdate.getElementDirect( i ) );
                playerList.appendElementString( playerString );
                delete [] playerString;
                }
            
            char *playerListString = playerList.getElementString();

            AppLog::infoF( "%d/%d players were sent part of a %d-line PU: %s",
                           playersReceivingPlayerUpdate.size(),
                           numLive, newUpdates.size(),
                           playerListString );
            
            delete [] playerListString;
            }
        

        for( int u=0; u<newUpdates.size(); u++ ) {
            UpdateRecord *r = newUpdates.getElement( u );
            delete [] r->formatString;
            }
        
        for( int u=0; u<newDeleteUpdates.size(); u++ ) {
            UpdateRecord *r = newDeleteUpdates.getElement( u );
            delete [] r->formatString;
            }

        
        if( lineageMessage != NULL ) {
            delete [] lineageMessage;
            }
        if( cursesMessage != NULL ) {
            delete [] cursesMessage;
            }
        if( namesMessage != NULL ) {
            delete [] namesMessage;
            }
        if( followingMessage != NULL ) {
            delete [] followingMessage;
            }
        if( exileMessage != NULL ) {
            delete [] exileMessage;
            }
        if( dyingMessage != NULL ) {
            delete [] dyingMessage;
            }
        if( healingMessage != NULL ) {
            delete [] healingMessage;
            }
        if( emotMessage != NULL ) {
            delete [] emotMessage;
            }
        if( wiggleMessage != NULL ) {
            delete [] wiggleMessage;
            }
        
        
        newOwnerStrings.deallocateStringElements();
        
        
        // these are global, so we must clear it every loop
        newSpeechPos.deleteAll();
        newSpeechPlayerIDs.deleteAll();
        newSpeechCurseFlags.deleteAll();
        newSpeechPhrases.deallocateStringElements();

        newLocationSpeech.deallocateStringElements();
        newLocationSpeechPos.deleteAll();

        newGraves.deleteAll();
        newGraveMoves.deleteAll();
        
        
        newEmotPlayerIDs.deleteAll();
        newEmotIndices.deleteAll();
        newEmotTTLs.deleteAll();
        

        
        // handle end-of-frame for all players that need it
        const char *frameMessage = "FM\n#";
        int frameMessageLength = strlen( frameMessage );
        
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *nextPlayer = players.getElement(i);
            
            if( nextPlayer->gotPartOfThisFrame && nextPlayer->connected ) {
                int numSent = 
                    nextPlayer->sock->send( 
                        (unsigned char*)frameMessage, 
                        frameMessageLength,
                        false, false );

                if( numSent != frameMessageLength ) {
                    setPlayerDisconnected( nextPlayer, "Socket write failed" );
                    }
                }
            nextPlayer->gotPartOfThisFrame = false;
            }
        

        
        // handle closing any that have an error
        for( int i=0; i<players.size(); i++ ) {
            LiveObject *nextPlayer = players.getElement(i);

            if( nextPlayer->error && nextPlayer->deleteSent &&
                nextPlayer->deleteSentDoneETA < Time::getCurrentTime() ) {
                AppLog::infoF( "Closing connection to player %d on error "
                               "(cause: %s)",
                               nextPlayer->id, nextPlayer->errorCauseString );

                AppLog::infoF( "%d remaining player(s) alive on server ",
                               players.size() - 1 );
                
                addPastPlayer( nextPlayer );

                if( nextPlayer->sock != NULL ) {
                    sockPoll.removeSocket( nextPlayer->sock );
                
                    delete nextPlayer->sock;
                    nextPlayer->sock = NULL;
                    }
                
                if( nextPlayer->sockBuffer != NULL ) {
                    delete nextPlayer->sockBuffer;
                    nextPlayer->sockBuffer = NULL;
                    }
                
                delete nextPlayer->lineage;
                
                delete nextPlayer->ancestorIDs;
                
                nextPlayer->ancestorEmails->deallocateStringElements();
                delete nextPlayer->ancestorEmails;
                
                nextPlayer->ancestorRelNames->deallocateStringElements();
                delete nextPlayer->ancestorRelNames;
                
                delete nextPlayer->ancestorLifeStartTimeSeconds;
                

                if( nextPlayer->name != NULL ) {
                    delete [] nextPlayer->name;
                    }

                if( nextPlayer->familyName != NULL ) {
                    delete [] nextPlayer->familyName;
                    }

                if( nextPlayer->lastSay != NULL ) {
                    delete [] nextPlayer->lastSay;
                    }
                
                freePlayerContainedArrays( nextPlayer );
                
                if( nextPlayer->pathToDest != NULL ) {
                    delete [] nextPlayer->pathToDest;
                    }

                if( nextPlayer->email != NULL ) {
                    delete [] nextPlayer->email;
                    }
                if( nextPlayer->origEmail != NULL  ) {
                    delete [] nextPlayer->origEmail;
                    }

                if( nextPlayer->murderPerpEmail != NULL ) {
                    delete [] nextPlayer->murderPerpEmail;
                    }

                if( nextPlayer->deathReason != NULL ) {
                    delete [] nextPlayer->deathReason;
                    }
                
                delete nextPlayer->babyBirthTimes;
                delete nextPlayer->babyIDs;

                players.deleteElement( i );
                i--;
                }
            }


        if( players.size() == 0 && newConnections.size() == 0 ) {
            if( shutdownMode ) {
                AppLog::info( "No live players or connections in shutdown " 
                              " mode, auto-quitting." );
                quit = true;
                }
            }
        }
    
    // stop listening on server socket immediately, before running
    // cleanup steps.  Cleanup may take a while, and we don't want to leave
    // server socket listening, because it will answer reflector and player
    // connection requests but then just hang there.

    // Closing the server socket makes these connection requests fail
    // instantly (instead of relying on client timeouts).
    delete server;

    quitCleanup();
    
    
    AppLog::info( "Done." );

    return 0;
    }



// implement null versions of these to allow a headless build
// we never call drawObject, but we need to use other objectBank functions


void *getSprite( int ) {
    return NULL;
    }

char *getSpriteTag( int ) {
    return NULL;
    }

char isSpriteBankLoaded() {
    return false;
    }

char markSpriteLive( int ) {
    return false;
    }

void stepSpriteBank() {
    }

void drawSprite( void*, doublePair, double, double, char ) {
    }

void setDrawColor( float inR, float inG, float inB, float inA ) {
    }

void setDrawColor( FloatColor inColor ) {
    }

void setDrawFade( float ) {
    }

float getTotalGlobalFade() {
    return 1.0f;
    }

void toggleAdditiveTextureColoring( char inAdditive ) {
    }

void toggleAdditiveBlend( char ) {
    }

void drawSquare( doublePair, double ) {
    }

void startAddingToStencil( char, char, float ) {
    }

void startDrawingThroughStencil( char ) {
    }

void stopStencil() {
    }





// dummy implementations of these functions, which are used in editor
// and client, but not server
#include "../gameSource/spriteBank.h"
SpriteRecord *getSpriteRecord( int inSpriteID ) {
    return NULL;
    }

#include "../gameSource/soundBank.h"
void checkIfSoundStillNeeded( int inID ) {
    }



char getSpriteHit( int inID, int inXCenterOffset, int inYCenterOffset ) {
    return false;
    }


char getUsesMultiplicativeBlending( int inID ) {
    return false;
    }


void toggleMultiplicativeBlend( char inMultiplicative ) {
    }


void countLiveUse( SoundUsage inUsage ) {
    }

void unCountLiveUse( SoundUsage inUsage ) {
    }



// animation bank calls these only if lip sync hack is enabled, which
// it never is for server
void *loadSpriteBase( const char*, char ) {
    return NULL;
    }

void freeSprite( void* ) {
    }

void startOutputAllFrames() {
    }

void stopOutputAllFrames() {
    }

