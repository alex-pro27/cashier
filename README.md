# Управление ккт через Websocket

### данные передаются в формате:
```ts
{
    "event": "event_name",  // Событие
    "data": any             // Данные 
}
```
## Ответ:
```ts
{
    "event": "onevent_name",   // событие
    "data": {
        "error": boolean,       // Ошибка (да/нет)
        "error_code": number    // Код ошибки
        "text": string,         // Текст ошибки
    }
}
```

## Примеры:

### [Открыть смену]:
`Запрос:`
```js
{
    event: "open_shift", 
    data: {
        cashier: "Сидр Петров" // Кассир
    }
}
```
`Ответ`
```js
{
	event: "onopen_shift",
	data: {
		error: false
	}
}
```

### [Завершение смены]:
`Запрос`
```js
{
    event: "close_shift", 
    data: {
        cashier: "Сидр Петров" // Кассир
    }
}
```
`Ответ`
```js
{
	event: "onclose_shift",
	data: {
		error: false,
		error_code: 0,
		cache_balance: 32440.5, 	// Количество денег в ящике
		fn_number: "0128010101", 	// Заводской номер фискального регистратора
		inn: "2147473636", 			// Инн
		shift_number: 45, 			// Номер смены
	}
}
```

### [Принудительное(Аварийное) завершение смены]:
`Запрос`
```js
{
    event: "force_close_shift", 
}
```
`Ответ`
```js
{
    event: "onforce_close_shift",
    data: {
        error: false,
        error_code: 0,
		cache_balance: 32440.5, 	// Количество денег в ящике
		fn_number: "0128010101", 	// Заводской номер фискального регистратора
		inn: "2147473636", 			// Инн
		shift_number: 45, 			// Номер смены
		check_number: 2, 			// Номер чека
		discount_sum: 0, 			// Скидка
		doc_number:	274				// Номер документа
    }
}
```

### [Новая транзакция]:
`Запрос`
```js
{
    event: "new_transaction", 
    data: {
        cashier: "Сидр Петров",     // Кассир
        doc_type: 2,                // Режим и тип документа (2-продажа, 3-возврат),
        pyment_type: 1,             // 0 - наличка, 1 - безнал
        rrn: "LINK_917113269868",   //опционально: ссылка безналичного платежа (если нужно сделать возврат)
        wares: [
            {
                name: "Водка Айс 0,5л 40%", // Наименование товара
                barcode: "123123",          // ШК товара
                tax_number: 0,              // НДС 0 - 20%, 1 - 10%,
                price: 123.45,              // Цена за 1 ед. измерения,
                quantity: 2,                // Количество
            }
        ]
    }
}
```
`Ответ`
```js
{
	event: "onnew_transaction",
	data: {
		error: false,           
		amount: 2469,               // Сумма для оплаты (в копейках)
		cashier: "Сидр Петров",     // Касир
		error_code: 0,              // Код ошибки
		text: "Успешно"             // Сообщение
		rrn: "LINK_917113269868",   // ссылка безналичного платежа 
		cache_balance: 32440.5, 	// Количество денег в ящике
		fn_number: "0128010101", 	// Заводской номер фискального регистратора
		inn: "2147473636", 			// Инн
		shift_number: 45, 			// Номер смены
	}
}
```

### [Отмена платежа]:
`Запрос`
```js
{
    event: "cancel_payment", 
    data: {
        rrn: "LINK_917113269868",   // ссылка безналичного платежа,
        amount: 2469,               // Сумма для оплаты (в копейках)
    }
}
```
`Ответ`
```js
{
	event: "oncancel_payment",
	data: {
		error: false,
		error_code: 0,
		text: "Успешно",
	}
}
```

### [Внесение/Изъятие из денежного ящика]:
`Запрос`
```js
{
    event: "cash_drawer_handler",
    data: {
        cashier: "Сидр Петров",     // Кассир,
        amount: 2469,               // сумма ввнесениы(в копейках)
        doc_type: 4                 // Режим и тип документа (4 - внесение, 5 - изъятие)
    }
}
```
`Ответ`
```js
{
	event: "oncash_drawer_handler",
	data: {
		error: false,
		error_code: 0,
		text: "Успешно",
	}
}
```

### [Получить информацию о ккт]:
`Запрос`
```js
{
    event: "get_kkt_info"
}
```
`Ответ`
```js
{
	event: "onget_kkt_info",
	data: {
		cache_balance: 32440.5, 	// Количество денег в ящике
		fn_number: "0128010101", 	// Заводской номер фискального регистратора
		inn: "2147473636", 			// Инн
		shift_number: 45, 			// Номер смены
	}
}
```
