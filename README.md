# Общие сведения

Игровая система состоит из приемника сигнала и кнопок. Приемник присоединяется к компьютеру по USB и нужен для коммуникации с кнопками.

Кнопки и примемник работают на базе контроллеров ESP32.

Для загрузки прошивки на контроллер используется Arduino IDE https://www.arduino.cc/en/software

После установки, нужно добавить в IDE поддержку ESP32 и библиотеку FastLED

## Принцип работы игровой системы

Приемник подключен по USB к компьютеру, на котором открыт сайт квизли и подключен при помощи Web Serial API. У приемника есть заданный ID (SET_ID).

У каждой кнопки задан ID приемника (SET_RECEIVER_ID) и когда они включены происходит пейринг кнопок и приемника.

На примемник приходит сигнал о нажатии кнопок, который он транслирует по USB на сайт.

Сайт может посылать сообщения на приемник, которые он передает кнопкам.

## Настройка игровой системы

1. Задать ID применика (отравить команду SET_ID:$RECEIVER_ID на приемник через USB).
2. Задать различные BUTTON_ID кнопкам (команда SET_BUTTON_ID:$BUTTON_ID).
3. Задать кнопкам ID приемника (отправить команду SET_RECEIVER_ID:$RECEIVER_ID).

Примечание: такая настройка обеспечивает возможность менять приемник, таким образом комбинируя кнопки в разных наборах.

## Отправка команд на кнопки

Есть 4 варианта отправки команд на кнопки:

- через приемник командой BROADCAST:$BUTTON_COMMAND. Команда отправится ВСЕМ кнопкам в раудисе действия приемника, даже тем, которые не спарены с приемником.
- через приемник командой PAIRED_BUTTONS:$BUTTON_COMMAND. Команда отправится ВСЕМ кнопкам, спареным с приемником.
- через приемник командой BUTTON:$BUTTON_ID:$BUTTON_COMMAND. Команда отправится кнопке с заданым $BUTTON_ID
- через USB

## Исходяшие команды кнопок

- BUTTON_PRESS:$BUTTON_ID

## Входяшие команды кнопок

- ENABLE - кнопка активна и отсылает BUTTON_PRESS при нажатии

- DISABLE - кнопка неактивна и не отсылает BUTTON_PRESS при нажатии

- BLOCK - кнопка заблокирована и не реагирует на команды ENABLE/DISABLE (нужно для блокировки неправильно ответивших игроков)

- UNBLOCK - кнопка разблокирована и реагирует на команды ENABLE/DISABLE

- LED_ON - лампочка горит

- LED_OFF - лампочка не горит

- SET_BUTTON_ID - задать ID кнопки

- GET_BUTTON_ID - напечатать ID кнопки

- SET_RECEIVER_ID - задать ID приемника

- GET_RECEIVER_ID - получить ID приемника

- SET_LED_COLOR:$RED:$GREEN:$BLUE - задать цвет лампочки

- FLASH - поморгать текущим цветом

- RANDOM_COLOR_FLASH - поморгать рандомными цветами

## Входяшие команды приемника

- SET_ID - задать ID приемника

- GET_ID - напечатать ID приемника

- BROADCAST:$BUTTON_COMMAND

- PAIRED_BUTTONS:$BUTTON_COMMAND

- BUTTON:$BUTTON_ID:$BUTTON_COMMAND

---

# General Information

The game system consists of a signal receiver and buttons. The receiver connects to a computer via USB and is used to communicate with the buttons.

Both the buttons and the receiver are built on ESP32 controllers.

To flash firmware onto the controller, use the Arduino IDE: https://www.arduino.cc/en/software

After installation, you'll need to add ESP32 support and the FastLED library to the IDE.

## How the Game System Works

The receiver is connected via USB to a computer running the Quizly website, which communicates with it using the Web Serial API.

- **Baud rate:** 9600
- **Encoding:** UTF-8 text

The receiver has a configured ID (`SET_ID`). Each button has a receiver ID (`SET_RECEIVER_ID`) configured, and when powered on, the buttons and receiver automatically pair.

When a button is pressed, the receiver gets the signal and forwards it over USB to the website. The website can also send messages to the receiver, which then relays them to the buttons.

## System Setup

1. Set the receiver ID (send the command `SET_ID:$RECEIVER_ID` to the receiver via USB).
2. Assign unique `BUTTON_ID`s to each button (command: `SET_BUTTON_ID:$BUTTON_ID`).
3. Set the receiver ID on each button (command: `SET_RECEIVER_ID:$RECEIVER_ID`).

> **Note:** This setup allows swapping out the receiver, making it possible to mix and match buttons across different sets.

## Sending Commands to Buttons

There are 4 ways to send commands to buttons:

- Via the receiver using `BROADCAST:$BUTTON_COMMAND` — sends the command to **all buttons** within range of the receiver, even those not paired with it.
- Via the receiver using `PAIRED_BUTTONS:$BUTTON_COMMAND` — sends the command to **all buttons paired** with the receiver.
- Via the receiver using `BUTTON:$BUTTON_ID:$BUTTON_COMMAND` — sends the command to the button with the specified `$BUTTON_ID`.
- Directly via USB.

## Outgoing Button Commands

- `BUTTON_PRESS:$BUTTON_ID`

## Incoming Button Commands

| Command | Description |
|---|---|
| `ENABLE` | Button is active and sends `BUTTON_PRESS` when pressed |
| `DISABLE` | Button is inactive and does not send `BUTTON_PRESS` when pressed |
| `BLOCK` | Button is blocked and ignores `ENABLE`/`DISABLE` commands (used to lock out players who answered incorrectly) |
| `UNBLOCK` | Button is unblocked and responds to `ENABLE`/`DISABLE` commands again |
| `LED_ON` | LED is on |
| `LED_OFF` | LED is off |
| `SET_BUTTON_ID` | Set the button ID |
| `GET_BUTTON_ID` | Print the button ID |
| `SET_RECEIVER_ID` | Set the receiver ID |
| `GET_RECEIVER_ID` | Get the receiver ID |
| `SET_LED_COLOR:$RED:$GREEN:$BLUE` | Set the LED color |
| `FLASH` | Flash the current color |
| `RANDOM_COLOR_FLASH` | Flash random colors |

## Incoming Receiver Commands

| Command | Description |
|---|---|
| `SET_ID` | Set the receiver ID |
| `GET_ID` | Print the receiver ID |
| `BROADCAST:$BUTTON_COMMAND` | Send a command to all buttons in range |
| `PAIRED_BUTTONS:$BUTTON_COMMAND` | Send a command to all paired buttons |
| `BUTTON:$BUTTON_ID:$BUTTON_COMMAND` | Send a command to a specific button |
