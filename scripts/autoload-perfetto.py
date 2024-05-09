import re
import time
from playwright.sync_api import Playwright, sync_playwright, expect


def run(playwright: Playwright) -> None:
    browser = playwright.firefox.launch(headless=False)
    context = browser.new_context()
    page = context.new_page()
    page.goto("http://localhost:10000/")

    while True:
        page.reload()
        with page.expect_file_chooser() as fc_info:
            page.get_by_text("Open trace file").click()
        file_chooser = fc_info.value
        file_chooser.set_files("PROF-EVENT-LOGS.proto")
        time.sleep(10)
    page.pause()

with sync_playwright() as playwright:
    run(playwright)

