import pandas as pd
import hashlib


FILE_PATH = "Задание-3-данные.xlsx"

RUSSIAN_ALPHABET = ['а', 'б', 'в', 'г', 'д', 'е', 'ж', 
                'з', 'и', 'й', 'к', 'л', 'м', 'н', 'о', 
                'п', 'р', 'с', 'т', 'у', 'ф', 'х', 'ц', 
                'ч', 'ш', 'щ', 'ъ', 'ы', 'ь', 'э', 'ю', 
                'я']
ENGLISH_ALPHABET = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 
                'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 
                'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 
                'y', 'z',]

COMMON_DOMAINS = ['ru', 'com', 'biz', 'info', 'org', 'net', 'io', 'edu', 'gov']


def decode_phone(encoded_phone):
    for num in range(89000000000, 89999999999):
        if hashlib.sha1(str(num).encode()).hexdigest() == encoded_phone:
            return str(num)
        
    return None

def caesar(text, alph, key):
    res = ''
    for s in text:
        is_upper = s == s.upper()
        s = s.lower()
        if s in alph:
            s = alph[(alph.index(s) - key) % len(alph)]
        if is_upper:
            s = s.upper()
        res += s
    return res


def decode_email(encoded_email):
    for i in range(26):
        decoded_email = caesar(encoded_email, ENGLISH_ALPHABET, i)

        domain = decoded_email.split('@')[1].rsplit('.', 1)[1]
        if domain in COMMON_DOMAINS:
            return decoded_email, i
    return None, None


def main():
    df = pd.read_excel(FILE_PATH, header=1, index_col=0).reset_index(drop=True)

    df['Расшифрованный телефон'] = None
    df['Расшифрованный email'] = None
    df['Расшифрованный адрес'] = None
    df['Сдвиг'] = None

    for (idx, row) in df.iterrows():
        encoded_phone = row['Телефон']
        encoded_email = row['email']
        encoded_addr = row['Адрес']

        decoded_phone = decode_phone(encoded_phone)
        decoded_email, shift = decode_email(encoded_email)
        decoded_addr = caesar(encoded_addr, RUSSIAN_ALPHABET, shift)

        df.at[idx, 'Расшифрованный телефон'] = decoded_phone
        df.at[idx, 'Расшифрованный email'] = decoded_email
        df.at[idx, 'Расшифрованный адрес'] = decoded_addr
        df.at[idx, 'Сдвиг'] = shift

    df.to_csv('task3_answer.csv', index=False)


if __name__ == "__main__":
    main()
