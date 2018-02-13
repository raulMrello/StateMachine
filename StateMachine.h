/*
 * StateMachine.h
 *
 *  Versión: 13 Feb 2018
 *  Author: raulMrello
 *
 *	Framework para funcionalidades HSM
 */
 
#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include "mbed.h"
#include "Heap.h"

  
//---------------------------------------------------------------------------------
//- class State -------------------------------------------------------------------
//---------------------------------------------------------------------------------


class State{
public:

    /** Lista de resultados que puede devolver un manejador de estado */
    enum StateResult{
        HANDLED,
        IGNORED,
        TRANSITION
    };

    /** Lista de eventos básicos en un estado */
    enum Event_type{
        EV_ENTRY         = (1<<0),  /// Evento al entrar en un estado
        EV_EXIT          = (1<<1),  /// Evento al salir de un estado
        EV_TIMED         = (1<<2),  /// Evento al cumplir el timeout de espera
        EV_INVALID       = (1<<3),  /// Evento al obtener un mensaje inválido
        EV_RESERVED_USER = (1<<4)   /// Eventos reservados al usuario
    };
    
    /** Estructura de eventos soportado por los estados */
    struct StateEvent{
        Event_type evt;
        osEvent*   oe;
    };

    /** Estructura de mensajes encolados soportado por los estados */
    struct Msg{
        uint32_t sig;
        void*    msg;
    };
  
    /** definición de un manejador de eventos como un puntero a función */
    typedef Callback<State::StateResult(State::StateEvent*)> EventHandler;

    /** Constructor
     *  Asigna un número de manejadores de eventos máximo (por defecto 3) sin contar
     *  los manejadores de eventos Entry, Exit, Timed.
     *  @param num_hnd Número máximo de manejadores de eventos
     */
    State(){
        _handler = callback(this, &State::defaultHandler);
    }
    
    /** setHandler()
     *  Inserta un nuevo manejador de eventos
     *  @param evhnd Manejador a instalar
     */
	void setHandler(EventHandler evhnd){
        _handler = evhnd;
    }
    
    /** getHandler()
     *  Obtiene el manejador del evento especificado
     *  @return manejador instalado, NULL si no instalado
     */
	EventHandler* getHandler(){
        return &_handler;    
    }      
            
protected:
    EventHandler _handler;
    State::StateResult defaultHandler(State::StateEvent*){ return IGNORED; }
};





//---------------------------------------------------------------------------------
//- class StateMachine ------------------------------------------------------------
//---------------------------------------------------------------------------------

#define NullState   (State*)0


class StateMachine {

public:

    /** StateMachine()
    *  Constructor por defecto
     */
    StateMachine() : _entryMsg((State::Msg){State::EV_ENTRY, 0}),
                     _exitMsg((State::Msg){State::EV_EXIT, 0}),
                     _timedMsg((State::Msg){State::EV_TIMED, 0}),
                     _invalidMsg((State::Msg){State::EV_INVALID, 0}){
        _put_cb = 0;
        _curr = NullState;
        _next = NullState;
        _parent = NullState;
    }

    
    /** attachMessageHandler()
    *  Instala callback para postear mensajes en caso de utilizar un Mail o Queue externos. Por defecto, si ésta
    *  instalación no se lleva a cabo, se trabajará únicamente con señales.
    *  @param putMsgCb Callback para publicar mensajes, en el caso de que utilice un Mail o Queue externo. Si no
    *         utilizará por defecto Signals.
    */
    void attachMessageHandler(Callback<void(State::Msg*)> *putMsgCb=0){
        _put_cb = putMsgCb;
    }
    
    
    /** run()
     *  Ejecuta la máquina de estados
     */
    void run(osEvent* oe){
        State::StateEvent se;
        se.oe = oe;
        if(oe->status == osEventTimeout){
            se.evt = State::EV_TIMED;
            invokeHandler(&se);
        }
        else if(oe->status == osEventMail || oe->status == osEventMessage){
            se.evt = (State::Event_type)((State::Msg*)oe->value.p)->sig;                    
            invokeHandler(&se);
        }
        else if(oe->status == osEventSignal){   
            uint32_t mask = 1;
            uint32_t sig = oe->value.signals;
            do{
                if((se.evt = (State::Event_type)(sig & mask)) != (State::Event_type)0){
                    invokeHandler(&se);                                    
                }
                // borro el flag procesado
                sig &= ~(mask);
                // paso al siguiente evento
                mask = (mask << 1);
            }while (sig != 0); 
        }         
    }    
     
    /** initState()
     *  Inicia la máquina de estados a un estado por defecto
     *  @param st Estado al que conmutar
     *  @parm tid Contexto thread sobre el que notificar
     *
     */
    void initState(State* st, osThreadId tid = 0){
        if(!tid){
            tid = osThreadGetId();
        }
        _curr = st;
        _next = NullState;
        raiseEvent(State::EV_ENTRY, tid);        
    }
     
    /** tranState()
     *  Cambia de estado (transición). En caso de ser el primero, fuerza su inicio
     *  @param st Estado al que conmutar
     *  @parm tid Contexto thread sobre el que notificar
     *
     */
    void tranState(State* st, osThreadId tid = 0){
        if(!tid){
            tid = osThreadGetId();
        }
        if(_curr == NullState){
            _curr = st;
            _next = NullState;
            raiseEvent(State::EV_ENTRY, tid);
        }
        else{
            _next = st;
            raiseEvent(State::EV_EXIT, tid);            
        }
    }
     
    /** nextState()
     *  Cambia al siguiente estado
     *  @param True cambio efectuado
     */
    bool nextState(){
        if(_next == NullState){
            return false;
        }
        _curr = NullState;
        tranState(_next); 
        return true;
    }
     
    /** setParent()
     *  Cambia de estado padre
     *  @param st Estado padre
     */
    void setParent(State* st){
        _parent = st;
    }
     
    /** raiseEvent()
     *  Lanza Evento
     *  @param evt Evento
     *  @parm tid Contexto thread sobre el que notificar
     *  @return Puntero a la zona de memoria reservada, si es necesario (liberar externamente cuando toque)
     */
    State::Msg* raiseEvent(uint32_t evt, osThreadId tid = 0){
        // si sólo utiliza señales...
        if(!_put_cb){
            if(!tid){
                tid = osThreadGetId();
            }
            osSignalSet(tid, evt);
            return 0;
        }
        switch(evt){
            case State::EV_ENTRY:{
                _put_cb->call((State::Msg*)&_entryMsg);
                return 0;
            }
            case State::EV_EXIT:{
                _put_cb->call((State::Msg*)&_exitMsg);
                return 0;
            }
            case State::EV_TIMED:{
                _put_cb->call((State::Msg*)&_timedMsg);
                return 0;
            }
            case State::EV_INVALID:{
                _put_cb->call((State::Msg*)&_invalidMsg);
                return 0;
            }
            default:{
                State::Msg* pmsg = (State::Msg*)Heap::memAlloc(sizeof(State::Msg));
                if(!pmsg){
                    return 0;
                }
                pmsg->sig = evt;
                pmsg->msg = 0;
                _put_cb->call(pmsg);
                return pmsg;
            }
        }
    } 
         
private:

    const State::Msg _entryMsg;
    const State::Msg _exitMsg;
    const State::Msg _timedMsg;
    const State::Msg _invalidMsg;    

    Callback<void(State::Msg*)> *_put_cb;
    State*    _curr;
    State*    _next;
    State*    _parent;

    
     
    /** invokeHandler()
     *  Invoca al manejador que corresponda o en su defecto al padre
     *  @param se Evento de estado a procesar
     */
    void invokeHandler(State::StateEvent* se){
        bool skip_parent = false;
        State::EventHandler* hnd;
        if(_curr != NullState){
            hnd = _curr->getHandler();
            if(hnd->call(se) != State::IGNORED){
                skip_parent = true;
            }
        }
        if(!skip_parent && _parent != NullState){
            hnd = _parent->getHandler();
            hnd->call(se);                        
        } 
    }
};



#endif
