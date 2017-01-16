
install(FILES preset.conf
  DESTINATION ${LIB_SYSTEMD_DIR}/system-preset
  RENAME 90-srund.preset
  COMPONENT config
)
install(FILES rsyslog.conf
  DESTINATION /etc/rsyslog.d
  RENAME srund.conf
  COMPONENT config
)
install(FILES logrotate
  DESTINATION /etc/logrotate.d
  RENAME srund
  COMPONENT config
)

configure_file(service.cmake srund.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/srund.service
  DESTINATION ${LIB_SYSTEMD_DIR}/system
  COMPONENT binary
)
