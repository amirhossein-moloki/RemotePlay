import asyncio
from playwright.async_api import async_playwright
import os

async def capture():
    async with async_playwright() as p:
        browser = await p.chromium.launch()
        page = await browser.new_page(viewport={'width': 1280, 'height': 720})
        # Use absolute path for the file
        path = os.path.abspath("mock_ui.html")
        await page.goto(f"file://{path}")
        # Wait a bit for rendering
        await asyncio.sleep(1)
        await page.screenshot(path="ui_screenshot.png")
        await browser.close()
        print("Screenshot saved to ui_screenshot.png")

if __name__ == "__main__":
    asyncio.run(capture())
