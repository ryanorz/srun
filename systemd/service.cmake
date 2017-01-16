[Unit]
Description=srund Daemon
After=syslog.target

[Service]
Type=forking
User=root
Group=root
RuntimeDirectory=srund
PIDFile=/run/srund/srund.pid
ExecStart=@CMAKE_INSTALL_FULL_SBINDIR@/srund
ExecStop=/bin/kill -TERM $MAINPID
Restart=on-failure

[Install]
WantedBy=multi-user.target
