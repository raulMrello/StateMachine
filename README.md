# StateMachine

StateMachine es un framework que proporciona funcionalidades de máquinas de estados jerárquicas HSM en C++.

- Versión 21 Feb 2018
  
  
## Changelog

*07.03.2018*
>**"Cambio callback put_cb para devolver osStatus"**
>
- [x] @7Mar2018.001 Cambio resultado para mejorar control de mutex, en caso de timeout.
  


*21.02.2018*
>**"Gestión de eventos IGNORED que deben invocar a Heap::memFree (bug.002 corregido)"**
>
- [x] @21Feb2018.003 En la rutina StateMachine::run, cuando un Mail o un Message es procesado mediante 'invokeHandler', se liberan los recursos 'Heap::memFree' del mensaje State::Msg y de su contenido State::Msg::msg si es que existen. De esta forma, se liberan también los recursos de mensajes ignorados. Esto implica que en la implementación de la máquina de estados concreta, ya no es necesario liberar los recursos ya que el propio framework lo hará.
  


*13.02.2018*
>**"Compatibilidad MBED y ESP-IDF"**
>
- [x] Compatibilizo para que sea funcional en ambas plataformas.
  

