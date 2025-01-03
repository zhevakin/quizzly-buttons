# Helpers

- Board name ESP32-WROOM-DA from esp32 packege 

### Used libraries:
- FastLED

# Comands description
Команды поступают на приемник-передатчик (esb-receiver) с компьютера по виртуальному com-порту. 
Передатчик проверяет проверяет что команда начинается с **BUTTON_** и в случае успеха отправляет в эфир.


### BUTTON_GET_ID
При получении этой команды, кнопка отвечает **BUTTON_ID:btn_id**, где *btn_id* - ID кнопки.
Все получившие команду кнопки вернут свой ID.



### BUTTON_SET_ID:btn_id
Команда задаёт ID кнопки. Полученное значение сохраняется в энергонезависимую память и загружается при включении. 
Кнопки не проверяют наличие ID в команде, поэтому все услышавшие эту команду кнопки сменят свой ID.

*btn_id* - String, должна быть уникальна для каждой кнопки;


### BUTTON_LED_COLOR:btn_id:colorRed:colorGreen:colorBlue
Команда задаёт цвет кнопки. Цвет сохраняется в энергонезависимую память и загружается при включении.

*btn_id* - ID кнопки, цвет которой нужно задать;

*colorRed* - яркость свечения красным. Byte (целое от 0 до 255);

*colorGreen* - яркость свечения зелёным. Byte (целое от 0 до 255);

*colorBlue* - яркость свечения синим. Byte (целое от 0 до 255);


### BUTTON_LED_ON:btn_id
Команда включает подсветку.

*btn_id* - ID кнопки.

### BUTTON_LED_OFF:btn_id
Команда выключает подсветку.

*btn_id* - ID кнопки.

### BUTTON_WINNER_FLASH:btn_id
При получении этой команды, кнопка начинает мигать разными цветами ~6 секунд. В конце гаснет.

*btn_id* - ID кнопки.