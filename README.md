# statement2csv
This is a Unix-style filter program that extracts transaction records from 
PDF bank statements and converts them to CSV format for easy import into MySQL.

## Bank Statement
Currently tested with statements issued by
- Commonwealth Bank of Australia
- National Australia Bank

Feel free to send other statements if you'd like support for additional formats.


## Installation
This tool requires `pdftotext`, so please ensure the `poppler-utils` package 
is installed beforehand.

Clone the repo:
```
git clone https://github.com/xuminic/statement2csv
```
Build the program:
```
make
```

## Quick Start
- basic usage

  `statement2csv` reads from a text stream, so the PDF must first be converted 
  using `pdftotext`. The steps are as follows:
  ```
  pdftotext -nodiag -nopgbrk -layout S20240331.pdf
  statement2csv -o S20240331.csv S20240331.txt 
  ```
  or simply in one step:
  ```
  pdftotext -nodiag -nopgbrk -layout S20240331.pdf - | statement2csv > S20240331.txt
  ```

  The `-o` option specifies the name of the output file. 
  If omitted, the output defaults to standard output (`stdout`).

- remove the table head
  
  `statement2csv` prints the head line by default:
  ```
  DateAcc,DateTra,Transaction,AccNo,Debit,Credit,Balance
  ```
  which can be suppressed by `-n` or `--no-title` option.
  ```
  $ pdftotext -nodiag -nopgbrk -layout S20240331.pdf - | statement2csv -n
  2024-1-1,, OPENING BALANCE,,,,
  ...
  ```

- choose columns of the csv table

  The columns of the output CSV can be selected by `-c` or `--column` option.
  The contents and order are decided by the column name and order.
  ```
  $ pdftotext -nodiag -nopgbrk -layout S20240331.pdf - | statement2csv -c Transaction,Debit,DateAcc
  1 Jan 2024 - 31 Mar 2024
  Transaction,Debit,DateAcc
  MYTRANSACTION AU Card xxxx,7.77,2024-1-3
  ...
  ```
  Note: When selecting columns, at least one of `Debit`, `Credit`, or `Balance` must be included.

- choose the timestamp format

  The timestamp format can be customized by `-t` or `--format` option. 
  This option interprets `Y` as year, `M` as month, `D` as day, 'H' as hour, 
  'm' as minute, 'S' as second, and treats any other character as a separator.
  ```
  $ pdftotext -nodiag -nopgbrk -layout S20240331.pdf - | statement2csv -t 'M/D:Y' -n
  1/1:2024,, OPENING BALANCE,,,,
  ...
  ```
  For example:
  - SQLite timestamp: `statement2csv -t Y-M-D`
  - MS Excel timestamp: `statement2csv -t D/M/Y`

## Use Case
The use case might not exist -- who would go through their bank statements anyway.

