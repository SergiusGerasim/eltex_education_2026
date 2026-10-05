# Системное программирование в Linux

Учебные работы по системному программированию, Linux,
межпроцессному взаимодействию, компьютерным сетям и модулям ядра.

## Структура репозитория

```text
.
├── 01-linux-administration/
│   ├── 01-shell-and-screen/
│   ├── 02-users-permissions-logs-and-git/
│   └── 03-processes-signals-and-job-control/
├── 02-c-programming/
│   ├── 02-01-phone-book/
│   ├── 02-02-calculator/
│   ├── 03-01-file-permission-bits/
│   ├── 03-02-ipv4-subnet-checker/
│   ├── 04-01-doubly-linked-phone-book/
│   ├── 04-02-priority-queue/
│   ├── 04-03-avl-tree-phone-book/
│   ├── 06-01-static-phone-book-library/
│   ├── 06-02-shared-phone-book-library/
│   └── 06-03-calculator-plugins/
├── 03-ipc-and-network-programming/
│   ├── 01-pipe-file-copier/
│   ├── 02-system-v-message-broker/
│   ├── 03-posix-message-queue-chat/
│   ├── 04-system-v-shared-memory/
│   ├── 05-posix-shared-memory/
│   ├── 06-udp-broadcast-chat/
│   ├── 07-tcp-chat-and-file-transfer/
│   ├── 08-raw-socket-packet-capture/
│   ├── cross-01-raw-udp-echo/
│   └── cross-02-taxi-process-manager/
├── 04-computer-networks/
│   ├── 01-basic-gns3-network/
│   ├── 02-spanning-tree-protocol/
│   ├── 03-vlan-and-802-1q/
│   ├── 04-dhcp-and-vlsm/
│   └── 05-rip-and-ospf-routing/
├── 05-linux-kernel-modules/
│   ├── 01-hello-kernel-module/
│   ├── 02-procfs-userspace-exchange/
│   ├── 03-keyboard-leds-sysfs/
│   ├── 04-character-device-exchange/
│   └── 05-netlink-userspace-exchange/
└── libs/
    └── Unity/
```

## Разделы

- [`01-linux-administration`](01-linux-administration) — командная строка, учётные записи, права доступа, процессы и сигналы.
- [`02-c-programming`](02-c-programming) — программы на C, структуры данных, битовые операции, статические и динамические библиотеки.
- [`03-ipc-and-network-programming`](03-ipc-and-network-programming) — процессы, IPC, очереди сообщений, разделяемая память и сокеты.
- [`04-computer-networks`](04-computer-networks) — лабораторные работы в GNS3: STP, VLAN, DHCP, RIP и OSPF.
- [`05-linux-kernel-modules`](05-linux-kernel-modules) — модули ядра Linux и обмен данными с userspace через procfs, sysfs, символьное устройство и Netlink.
- [`libs`](libs) — общие сторонние зависимости.
