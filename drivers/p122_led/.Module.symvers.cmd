cmd_/home/ubuntu/pi_bsp/drivers/p122_led/Module.symvers :=  sed 's/ko$$/o/'  /home/ubuntu/pi_bsp/drivers/p122_led/modules.order | scripts/mod/modpost -m -a    -o /home/ubuntu/pi_bsp/drivers/p122_led/Module.symvers -e -i Module.symvers -T - 